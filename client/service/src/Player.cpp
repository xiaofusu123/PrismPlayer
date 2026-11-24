#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

#include "PlayerImpl.h"

#include "SyncFactory.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <spdlog/spdlog.h>
#include <string>

namespace Prism::Service {

/* ========== 默认配置 ========== */

static PrismConfig default_config() {
    PrismConfig c{};
    c.video_output_width = 0;
    c.video_output_height = 0;
    c.audio_sample_rate = 0;
    c.default_volume = 1.0f;
    c.enable_video = true;
    c.enable_audio = true;
    c.log_level = "info";
    return c;
}

/* ========== 默认网络桩实现 ========== */

class DefaultNetworkClient : public IServiceNetwork {
  public:
    int login(const char* username, const char* password, const char* server_url) override {
        spdlog::info("[DefaultNetwork] login: user={} server={}", username, server_url);
        logged_in_ = true;
        return PRISM_OK;
    }
    int logout() override {
        spdlog::info("[DefaultNetwork] logout");
        logged_in_ = false;
        in_room_ = false;
        room_info_ = {};
        return PRISM_OK;
    }
    int join_room(const char* room_id) override {
        if (!logged_in_) {
            spdlog::warn("[DefaultNetwork] join_room rejected: not logged in");
            return PRISM_ERROR_NETWORK;
        }
        spdlog::info("[DefaultNetwork] join_room: {}", room_id);
        in_room_ = true;
        room_info_ = {};
        constexpr size_t MAX_ROOM_ID_LENGTH = sizeof(room_info_.room_id) - 1;
        size_t length = 0;
        for (; length < MAX_ROOM_ID_LENGTH && room_id[length] != '\0'; ++length) {
            room_info_.room_id[length] = room_id[length];
        }
        room_info_.room_id[length] = '\0';
        room_info_.member_count = 1;
        return PRISM_OK;
    }
    int leave_room() override {
        spdlog::info("[DefaultNetwork] leave_room");
        in_room_ = false;
        room_info_ = {};
        return PRISM_OK;
    }
    int get_room_info(PrismRoomInfo* info) override {
        if (!in_room_) {
            return PRISM_ERROR_NETWORK;
        }
        if (info) {
            *info = room_info_;
        }
        return PRISM_OK;
    }
    [[nodiscard]] PrismRoomState get_room_state() const override {
        return in_room_ ? PRISM_ROOM_CONNECTED : PRISM_ROOM_DISCONNECTED;
    }

  private:
    bool logged_in_{false};
    bool in_room_{false};
    PrismRoomInfo room_info_{};
};

/* ========== PrismPlayerInternal 实现 ========== */

PrismPlayerInternal::PrismPlayerInternal(const PrismConfig& cfg, PrismEventCallback cb, void* ud)
    : config_(cfg), log_level_(cfg.log_level ? cfg.log_level : "info"), callback_(cb),
      user_data_(ud) {
    volume_.store(config_.default_volume);
    media_info_.duration_ms = -1;

    sync_sm_ = Prism::Business::create_playback_state_machine();
    sync_algo_ = Prism::Business::create_sync_algorithm();
    cmd_dispatcher_ = Prism::Business::create_command_dispatcher();
    engine_observer_ = Prism::Business::create_engine_observer();

    // 阈值默认值唯一真源为 SyncConfig；运行期可经 ISyncAlgorithm::configure 覆盖。
    sync_algo_->configure(Prism::Business::SyncConfig{});

    engine_observer_->set_sync_algorithm(sync_algo_.get());
    engine_observer_->set_state_machine(sync_sm_.get());
    system_clock_ = Prism::Business::create_sync_clock();
    engine_observer_->set_clock(system_clock_.get());
    cmd_dispatcher_->set_frame_observer(engine_observer_.get());

    network_ = std::make_unique<DefaultNetworkClient>();

    spdlog::set_level(spdlog::level::from_str(log_level_));
    spdlog::info("[PrismPlayer] instance created");
}

PrismPlayerInternal::~PrismPlayerInternal() {
    if (engines_initialized_) {
        cmd_dispatcher_->shutdown_engines();
    }
    spdlog::info("[PrismPlayer] instance destroyed");
}

void PrismPlayerInternal::fire_event(PrismEventType type, const void* data) const {
    if (callback_) {
        callback_(type, data, user_data_);
    }
}

/* ========== 引擎初始化辅助 ========== */

static bool init_engines(PrismPlayerInternal* p) {
    if (p->engines_initialized_)
        return true;

    if (!p->cmd_dispatcher_->initialize_engines(p->audio_factory_, p->video_factory_,
                                                p->config_.enable_video)) {
        spdlog::error("[PrismPlayer] failed to init engines");
        p->last_error_.store(PRISM_ERROR_UNKNOWN);
        return false;
    }

    p->engines_initialized_ = true;
    return true;
}

/* ========== 值钳位辅助 ========== */

static float clamp(float val, float lo, float hi) {
    // 用 std::clamp 而非 std::max/std::min：Windows SDK 的 minwindef.h 会把后者
    // 展开成宏，导致 `std::max(...)` 在 MSVC target 下编译失败。
    return std::clamp(val, lo, hi);
}

/* ========== 同步状态机辅助 ========== */

static void enter_playing_sync(PrismPlayerInternal* p) {
    if (p->sync_sm_->get_state() == Prism::Business::SyncState::UNINIT) {
        p->sync_sm_->transition(Prism::Business::SyncState::CALIBATING);
        return;
    }
    p->sync_sm_->transition(Prism::Business::SyncState::SYNCHRONIZED);
}

} // namespace Prism::Service

/* ========================================================================
 *  C API 实现
 * ======================================================================== */

extern "C" {

/* ========== 生命周期 ========== */

PRISM_API PrismPlayerHandle prism_player_create(const PrismConfig* config,
                                                PrismEventCallback callback, void* user_data) {
    PrismConfig cfg = config ? *config : Prism::Service::default_config();

    auto* p = new (std::nothrow) Prism::Service::PrismPlayerInternal(cfg, callback, user_data);
    if (!p) {
        spdlog::error("[PrismPlayer] failed to allocate instance");
        return nullptr;
    }

    spdlog::info("[PrismPlayer] handle created");
    return static_cast<PrismPlayerHandle>(p);
}

PRISM_API void prism_player_destroy(PrismPlayerHandle player) {
    if (!player)
        return;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    p->network_->leave_room();

    delete p;
    spdlog::info("[PrismPlayer] handle destroyed");
}

/* ========== 依赖注入 ========== */

PRISM_API int prism_player_set_audio_factory(PrismPlayerHandle player, void* factory) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;
    if (!factory) {
        static_cast<Prism::Service::PrismPlayerInternal*>(player)->last_error_.store(
            PRISM_ERROR_INVALID_PARAM);
        return PRISM_ERROR_INVALID_PARAM;
    }
    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    p->audio_factory_ = factory;
    spdlog::info("[PrismPlayer] audio factory injected");
    return PRISM_OK;
}

PRISM_API int prism_player_set_video_factory(PrismPlayerHandle player, void* factory) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;
    if (!factory) {
        static_cast<Prism::Service::PrismPlayerInternal*>(player)->last_error_.store(
            PRISM_ERROR_INVALID_PARAM);
        return PRISM_ERROR_INVALID_PARAM;
    }
    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    p->video_factory_ = factory;
    spdlog::info("[PrismPlayer] video factory injected");
    return PRISM_OK;
}

PRISM_API int prism_player_set_network_client(PrismPlayerHandle player, void* network) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;
    if (!network) {
        static_cast<Prism::Service::PrismPlayerInternal*>(player)->last_error_.store(
            PRISM_ERROR_INVALID_PARAM);
        return PRISM_ERROR_INVALID_PARAM;
    }
    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    p->network_.reset(static_cast<Prism::Service::IServiceNetwork*>(network));

    spdlog::info("[PrismPlayer] network client injected");
    return PRISM_OK;
}

/* ========== 用户认证 ========== */

PRISM_API int prism_player_login(PrismPlayerHandle player, const PrismLoginParams* params) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;
    if (!params || !params->username || !params->password || !params->server_url) {
        spdlog::warn("[PrismPlayer] login failed: invalid parameters");
        static_cast<Prism::Service::PrismPlayerInternal*>(player)->last_error_.store(
            PRISM_ERROR_INVALID_PARAM);
        return PRISM_ERROR_INVALID_PARAM;
    }

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    int ret = p->network_->login(params->username, params->password, params->server_url);
    if (ret == PRISM_OK) {
        p->fire_event(PRISM_EVENT_LOGIN_SUCCESS);
    } else {
        p->last_error_.store(static_cast<PrismErrorCode>(ret));
        p->fire_event(PRISM_EVENT_LOGIN_FAILED);
    }
    return ret;
}

PRISM_API int prism_player_logout(PrismPlayerHandle player) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    // 登出前自动离开房间（若仍在房间内），与接口注释的语义保持一致。
    if (p->network_->get_room_state() == PRISM_ROOM_CONNECTED) {
        int ret = p->network_->leave_room();
        if (ret != PRISM_OK) {
            p->last_error_.store(static_cast<PrismErrorCode>(ret));
            return ret;
        }
        p->fire_event(PRISM_EVENT_ROOM_LEFT);
    }
    int ret = p->network_->logout();
    if (ret != PRISM_OK) {
        p->last_error_.store(static_cast<PrismErrorCode>(ret));
    }
    return ret;
}

/* ========== 房间管理 ========== */

PRISM_API int prism_player_join_room(PrismPlayerHandle player, const char* room_id) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;
    if (!room_id) {
        static_cast<Prism::Service::PrismPlayerInternal*>(player)->last_error_.store(
            PRISM_ERROR_INVALID_PARAM);
        return PRISM_ERROR_INVALID_PARAM;
    }

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    int ret = p->network_->join_room(room_id);
    if (ret == PRISM_OK) {
        p->fire_event(PRISM_EVENT_ROOM_JOINED);
    } else {
        p->last_error_.store(static_cast<PrismErrorCode>(ret));
    }
    return ret;
}

PRISM_API int prism_player_leave_room(PrismPlayerHandle player) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    bool was_in_room = p->network_->get_room_state() == PRISM_ROOM_CONNECTED;
    int ret = p->network_->leave_room();
    if (ret != PRISM_OK) {
        p->last_error_.store(static_cast<PrismErrorCode>(ret));
        return ret;
    }
    if (was_in_room) {
        p->fire_event(PRISM_EVENT_ROOM_LEFT);
    }
    return ret;
}

PRISM_API int prism_player_get_room_info(PrismPlayerHandle player, PrismRoomInfo* info) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;
    if (!info) {
        static_cast<Prism::Service::PrismPlayerInternal*>(player)->last_error_.store(
            PRISM_ERROR_INVALID_PARAM);
        return PRISM_ERROR_INVALID_PARAM;
    }

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    int ret = p->network_->get_room_info(info);
    if (ret != PRISM_OK) {
        p->last_error_.store(static_cast<PrismErrorCode>(ret));
    }
    return ret;
}

PRISM_API PrismRoomState prism_player_get_room_state(PrismPlayerHandle player) {
    if (!player)
        return PRISM_ROOM_DISCONNECTED;
    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    return p->network_->get_room_state();
}

/* ========== 媒体源 ========== */

PRISM_API int prism_player_open(PrismPlayerHandle player, const char* uri) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;
    if (!uri) {
        static_cast<Prism::Service::PrismPlayerInternal*>(player)->last_error_.store(
            PRISM_ERROR_INVALID_PARAM);
        return PRISM_ERROR_INVALID_PARAM;
    }

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    p->state_.store(PRISM_STATE_LOADING);
    p->media_uri_ = uri;
    p->sync_sm_->reset();
    p->engine_observer_->reset();
    spdlog::info("[PrismPlayer] opening: {}", uri);

    if (!Prism::Service::init_engines(p)) {
        p->state_.store(PRISM_STATE_ERROR);
        p->sync_sm_->transition(Prism::Business::SyncState::SYNC_ERROR);
        p->last_error_.store(PRISM_ERROR_OPEN_FAILED);
        spdlog::error("[PrismPlayer] open failed: engine init error");
        return PRISM_ERROR_OPEN_FAILED;
    }

    p->state_.store(PRISM_STATE_PAUSED);
    p->sync_sm_->transition(Prism::Business::SyncState::CALIBATING);
    p->fire_event(PRISM_EVENT_MEDIA_LOADED);
    spdlog::info("[PrismPlayer] media loaded: {}", uri);
    return PRISM_OK;
}

PRISM_API int prism_player_close(PrismPlayerHandle player) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    p->cmd_dispatcher_->shutdown_engines();

    p->engines_initialized_ = false;
    p->media_uri_.clear();
    p->media_info_ = {};
    p->media_info_.duration_ms = -1;
    p->sync_sm_->reset();
    p->engine_observer_->reset();
    p->state_.store(PRISM_STATE_IDLE);

    spdlog::info("[PrismPlayer] closed");
    return PRISM_OK;
}

/* ========== 播放控制 ========== */

PRISM_API int prism_player_play(PrismPlayerHandle player) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    PrismState s = p->state_.load();

    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) {
        p->last_error_.store(PRISM_ERROR_NO_MEDIA);
        spdlog::warn("[PrismPlayer] play failed: no media loaded");
        return PRISM_ERROR_NO_MEDIA;
    }

    if (s == PRISM_STATE_PLAYING)
        return PRISM_OK;

    if (!Prism::Service::init_engines(p)) {
        p->state_.store(PRISM_STATE_ERROR);
        p->last_error_.store(PRISM_ERROR_UNKNOWN);
        spdlog::error("[PrismPlayer] play failed: engine init error");
        return PRISM_ERROR_UNKNOWN;
    }

    if (!p->cmd_dispatcher_->dispatch_play()) {
        p->state_.store(PRISM_STATE_ERROR);
        p->last_error_.store(PRISM_ERROR_UNKNOWN);
        spdlog::error("[PrismPlayer] play failed: engine dispatch failed");
        return PRISM_ERROR_UNKNOWN;
    }

    p->engine_observer_->set_paused(false);
    Prism::Service::enter_playing_sync(p);
    p->state_.store(PRISM_STATE_PLAYING);
    spdlog::info("[PrismPlayer] playing");
    return PRISM_OK;
}

PRISM_API int prism_player_pause(PrismPlayerHandle player) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    if (p->state_.load() != PRISM_STATE_PLAYING)
        return PRISM_OK;

    if (!p->cmd_dispatcher_->dispatch_pause()) {
        p->last_error_.store(PRISM_ERROR_UNKNOWN);
        spdlog::warn("[PrismPlayer] pause failed: engine dispatch failed");
        return PRISM_ERROR_UNKNOWN;
    }

    p->engine_observer_->set_paused(true);
    p->state_.store(PRISM_STATE_PAUSED);
    spdlog::info("[PrismPlayer] paused");
    return PRISM_OK;
}

PRISM_API int prism_player_stop(PrismPlayerHandle player) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    spdlog::info("[PrismPlayer] stopping");

    p->cmd_dispatcher_->dispatch_pause();
    p->cmd_dispatcher_->shutdown_engines();

    p->engines_initialized_ = false;
    p->sync_sm_->reset();
    p->engine_observer_->reset();
    p->state_.store(PRISM_STATE_STOPPED);
    spdlog::info("[PrismPlayer] stopped");
    return PRISM_OK;
}

PRISM_API int prism_player_seek(PrismPlayerHandle player, int64_t position_ms, PrismSeekMode mode) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    PrismState s = p->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) {
        p->last_error_.store(PRISM_ERROR_NO_MEDIA);
        spdlog::warn("[PrismPlayer] seek failed: no media loaded");
        return PRISM_ERROR_NO_MEDIA;
    }

    if ((mode != PRISM_SEEK_ABSOLUTE && mode != PRISM_SEEK_RELATIVE) ||
        (mode == PRISM_SEEK_ABSOLUTE && position_ms < 0)) {
        p->last_error_.store(PRISM_ERROR_INVALID_PARAM);
        spdlog::warn("[PrismPlayer] seek failed: invalid position or mode");
        return PRISM_ERROR_INVALID_PARAM;
    }

    // 引擎接口只接受绝对位置：相对模式先按当前音频时钟换算成绝对位置。
    int64_t target_ms = position_ms;
    if (mode == PRISM_SEEK_RELATIVE) {
        auto drift = p->sync_algo_->get_drift_info();
        int64_t cur = static_cast<int64_t>(drift.audio_pts);
        target_ms = std::max<int64_t>(0, cur + position_ms);
    }

    if (!p->cmd_dispatcher_->dispatch_seek(std::chrono::milliseconds{target_ms})) {
        p->last_error_.store(PRISM_ERROR_SEEK_FAILED);
        spdlog::warn("[PrismPlayer] seek failed: engine dispatch failed");
        return PRISM_ERROR_SEEK_FAILED;
    }

    // seek 后同步算法与观察者一并重置，不拿旧 PTS 计算巨大假漂移。
    p->sync_sm_->reset();
    p->engine_observer_->reset();
    if (s == PRISM_STATE_PAUSED) {
        p->engine_observer_->set_paused(true);
    }

    p->fire_event(PRISM_EVENT_SEEK_COMPLETED);
    spdlog::info("[PrismPlayer] seek to {}ms", target_ms);
    return PRISM_OK;
}

/* ========== 状态查询 ========== */

PRISM_API PrismState prism_player_get_state(PrismPlayerHandle player) {
    if (!player)
        return PRISM_STATE_ERROR;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->state_.load();
}

PRISM_API int64_t prism_player_get_position(PrismPlayerHandle player) {
    if (!player)
        return -1;
    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    PrismState s = p->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR)
        return -1;

    auto drift = p->sync_algo_->get_drift_info();
    return static_cast<int64_t>(drift.audio_pts);
}

PRISM_API int64_t prism_player_get_duration(PrismPlayerHandle player) {
    if (!player)
        return -1;
    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    PrismState s = p->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR)
        return -1;

    return p->media_info_.duration_ms;
}

/* ========== 音量控制 ========== */

PRISM_API int prism_player_set_volume(PrismPlayerHandle player, float volume) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    float clamped = Prism::Service::clamp(volume, 0.0f, 1.0f);
    p->volume_.store(clamped);
    spdlog::debug("[PrismPlayer] volume set to {:.2f}", clamped);
    return PRISM_OK;
}

PRISM_API float prism_player_get_volume(PrismPlayerHandle player) {
    if (!player)
        return 0.0f;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->volume_.load();
}

PRISM_API int prism_player_set_mute(PrismPlayerHandle player, bool mute) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    bool was_muted = p->mute_.exchange(mute);
    if (mute && !was_muted) {
        p->volume_before_mute_.store(p->volume_.load());
        spdlog::debug("[PrismPlayer] muted");
    } else if (!mute && was_muted) {
        p->volume_.store(p->volume_before_mute_.load());
        spdlog::debug("[PrismPlayer] unmuted");
    }

    return PRISM_OK;
}

PRISM_API bool prism_player_get_mute(PrismPlayerHandle player) {
    if (!player)
        return false;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->mute_.load();
}

/* ========== 播放属性 ========== */

PRISM_API int prism_player_set_playback_speed(PrismPlayerHandle player, float speed) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    float clamped = Prism::Service::clamp(speed, 0.5f, 2.0f);
    p->speed_.store(clamped);

    if (!p->cmd_dispatcher_->dispatch_speed(clamped)) {
        p->last_error_.store(PRISM_ERROR_UNKNOWN);
        spdlog::warn("[PrismPlayer] set speed failed: engine dispatch failed");
        return PRISM_ERROR_UNKNOWN;
    }

    spdlog::debug("[PrismPlayer] playback speed set to {:.2f}x", clamped);
    return PRISM_OK;
}

PRISM_API float prism_player_get_playback_speed(PrismPlayerHandle player) {
    if (!player)
        return 1.0f;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->speed_.load();
}

PRISM_API int prism_player_set_loop(PrismPlayerHandle player, bool loop) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;
    static_cast<Prism::Service::PrismPlayerInternal*>(player)->loop_.store(loop);
    spdlog::debug("[PrismPlayer] loop set to {}", loop);
    return PRISM_OK;
}

PRISM_API bool prism_player_get_loop(PrismPlayerHandle player) {
    if (!player)
        return false;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->loop_.load();
}

/* ========== 视频窗口 ========== */

PRISM_API int prism_player_set_video_window(PrismPlayerHandle player, void* native_window) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;
    static_cast<Prism::Service::PrismPlayerInternal*>(player)->video_window_.store(native_window);
    spdlog::debug("[PrismPlayer] video window set to {}", native_window);
    return PRISM_OK;
}

/* ========== 媒体信息 ========== */

PRISM_API int prism_player_get_media_info(PrismPlayerHandle player, PrismMediaInfo* info) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;
    if (!info) {
        static_cast<Prism::Service::PrismPlayerInternal*>(player)->last_error_.store(
            PRISM_ERROR_INVALID_PARAM);
        return PRISM_ERROR_INVALID_PARAM;
    }

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    PrismState s = p->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) {
        p->last_error_.store(PRISM_ERROR_NO_MEDIA);
        return PRISM_ERROR_NO_MEDIA;
    }

    *info = p->media_info_;
    return PRISM_OK;
}

/* ========== 诊断工具 ========== */

PRISM_API PrismErrorCode prism_player_get_last_error(PrismPlayerHandle player) {
    if (!player)
        return PRISM_ERROR_INVALID_HANDLE;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->last_error_.load();
}

PRISM_API const char* prism_player_get_version(void) {
    return "PrismPlayer " STRINGIFY(PRISM_VERSION_MAJOR) "." STRINGIFY(
        PRISM_VERSION_MINOR) "." STRINGIFY(PRISM_VERSION_PATCH);
}

} // extern "C"
