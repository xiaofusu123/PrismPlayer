#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

#include "PlayerImpl.h"

#include "AudioEngineWasapiSharedFactory.h"
#include "VideoEngineVulkanFactory.h"

#include "SyncFactory.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <spdlog/spdlog.h>

namespace Prism::Service {

/* ========== 默认配置 ========== */

static PrismConfig default_config()
{
    PrismConfig c{};
    c.video_output_width  = 0;
    c.video_output_height = 0;
    c.audio_sample_rate   = 0;
    c.default_volume      = 1.0f;
    c.enable_video        = true;
    c.enable_audio        = true;
    c.log_level           = "info";
    return c;
}

/* ========== 默认网络桩实现 ========== */

class DefaultNetworkClient : public IServiceNetwork {
public:
    int login(const char* username, const char* password, const char* server_url) override
    {
        spdlog::info("[DefaultNetwork] login: user={} server={}", username, server_url);
        return PRISM_OK;
    }
    int logout() override
    {
        spdlog::info("[DefaultNetwork] logout");
        return PRISM_OK;
    }
    int join_room(const char* room_id) override
    {
        spdlog::info("[DefaultNetwork] join_room: {}", room_id);
        return PRISM_OK;
    }
    int leave_room() override
    {
        spdlog::info("[DefaultNetwork] leave_room");
        return PRISM_OK;
    }
    int get_room_info(PrismRoomInfo* info) override
    {
        if (info) *info = {};
        return PRISM_OK;
    }
    PrismRoomState get_room_state() const override
    {
        return PRISM_ROOM_DISCONNECTED;
    }
};

/* ========== PrismPlayerInternal 实现 ========== */

PrismPlayerInternal::PrismPlayerInternal(const PrismConfig& cfg,
                                         PrismEventCallback cb,
                                         void* ud)
    : config_(cfg)
    , log_level_(cfg.log_level ? cfg.log_level : "info")
    , callback_(cb)
    , user_data_(ud)
{
    volume_.store(config_.default_volume);

    sync_sm_         = Prism::Business::create_playback_state_machine();
    sync_algo_       = Prism::Business::create_sync_algorithm();
    cmd_dispatcher_  = Prism::Business::create_command_dispatcher();
    engine_observer_ = Prism::Business::create_engine_observer();

    engine_observer_->set_sync_algorithm(sync_algo_.get());
    engine_observer_->set_state_machine(sync_sm_.get());

    network_ = std::make_unique<DefaultNetworkClient>();

    spdlog::set_level(spdlog::level::from_str(log_level_));
    spdlog::info("[PrismPlayer] instance created");
}

PrismPlayerInternal::~PrismPlayerInternal()
{
    if (engines_initialized_) {
        if (audio_engine_) {
            audio_engine_->close();
        }
        if (video_engine_) {
            video_engine_->close();
        }
    }
    spdlog::info("[PrismPlayer] instance destroyed");
}

void PrismPlayerInternal::fire_event(PrismEventType type, const void* data) const
{
    if (callback_) {
        callback_(type, data, user_data_);
    }
}

/* ========== 引擎初始化辅助 ========== */

static Prism::Engine::AudioEngineFactory& get_audio_factory(PrismPlayerInternal* p)
{
    if (p->audio_factory_) return *p->audio_factory_;

    static Prism::Engine::AudioEngineWasapiSharedFactory default_factory;
    return default_factory;
}

static Prism::Engine::VideoEngineFactory& get_video_factory(PrismPlayerInternal* p)
{
    if (p->video_factory_) return *p->video_factory_;

    static Prism::Engine::VideoEngineVulkanFactory default_factory;
    return default_factory;
}

static bool init_engines(PrismPlayerInternal* p)
{
    if (p->engines_initialized_) return true;

    if (!p->audio_engine_) {
        p->audio_engine_ = get_audio_factory(p).create_audio_engine();
        if (!p->audio_engine_ || !p->audio_engine_->init()) {
            spdlog::error("[PrismPlayer] failed to init audio engine");
            p->audio_engine_.reset();
            p->last_error_.store(PRISM_ERROR_UNKNOWN);
            return false;
        }
        p->cmd_dispatcher_->set_audio_engine(p->audio_engine_.get());
        spdlog::info("[PrismPlayer] audio engine initialized");
    }

    if (!p->video_engine_ && p->config_.enable_video) {
        p->video_engine_ = get_video_factory(p).create_audio_engine();
        if (!p->video_engine_ || !p->video_engine_->init()) {
            spdlog::error("[PrismPlayer] failed to init video engine");
            p->video_engine_.reset();
            p->last_error_.store(PRISM_ERROR_UNKNOWN);
            return false;
        }
        p->cmd_dispatcher_->set_video_engine(p->video_engine_.get());
        spdlog::info("[PrismPlayer] video engine initialized");
    }

    p->engines_initialized_ = true;
    return true;
}

/* ========== 值钳位辅助 ========== */

static float clamp(float val, float lo, float hi)
{
    return std::max(lo, std::min(hi, val));
}

} // namespace Prism::Service

/* ========================================================================
 *  C API 实现
 * ======================================================================== */

extern "C" {

/* ========== 生命周期 ========== */

_API PrismPlayerHandle prism_player_create(const PrismConfig* config,
                                           PrismEventCallback callback,
                                           void* user_data)
{
    PrismConfig cfg = config ? *config : Prism::Service::default_config();

    auto* p = new (std::nothrow) Prism::Service::PrismPlayerInternal(cfg, callback, user_data);
    if (!p) {
        spdlog::error("[PrismPlayer] failed to allocate instance");
        return nullptr;
    }

    spdlog::info("[PrismPlayer] handle created");
    return static_cast<PrismPlayerHandle>(p);
}

_API void prism_player_destroy(PrismPlayerHandle player)
{
    if (!player) return;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    p->network_->leave_room();

    delete p;
    spdlog::info("[PrismPlayer] handle destroyed");
}

/* ========== 依赖注入 ========== */

_API int prism_player_set_audio_factory(PrismPlayerHandle player, void* factory)
{
    if (!player || !factory) return PRISM_ERROR_INVALID_HANDLE;
    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    p->audio_factory_ = static_cast<Prism::Engine::AudioEngineFactory*>(factory);
    spdlog::info("[PrismPlayer] audio factory injected");
    return PRISM_OK;
}

_API int prism_player_set_video_factory(PrismPlayerHandle player, void* factory)
{
    if (!player || !factory) return PRISM_ERROR_INVALID_HANDLE;
    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    p->video_factory_ = static_cast<Prism::Engine::VideoEngineFactory*>(factory);
    spdlog::info("[PrismPlayer] video factory injected");
    return PRISM_OK;
}

_API int prism_player_set_network_client(PrismPlayerHandle player, void* network)
{
    if (!player || !network) return PRISM_ERROR_INVALID_HANDLE;
    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    p->network_.reset(static_cast<Prism::Service::IServiceNetwork*>(network));

    spdlog::info("[PrismPlayer] network client injected");
    return PRISM_OK;
}

/* ========== 用户认证 ========== */

_API int prism_player_login(PrismPlayerHandle player, const PrismLoginParams* params)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    if (!params || !params->username || !params->password || !params->server_url) {
        spdlog::warn("[PrismPlayer] login failed: invalid parameters");
        return PRISM_ERROR_INVALID_PARAM;
    }

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    int ret = p->network_->login(params->username, params->password, params->server_url);
    if (ret == PRISM_OK) {
        p->fire_event(PRISM_EVENT_LOGIN_SUCCESS);
    } else {
        p->fire_event(PRISM_EVENT_LOGIN_FAILED);
    }
    return ret;
}

_API int prism_player_logout(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    return p->network_->logout();
}

/* ========== 房间管理 ========== */

_API int prism_player_join_room(PrismPlayerHandle player, const char* room_id)
{
    if (!player)  return PRISM_ERROR_INVALID_HANDLE;
    if (!room_id) return PRISM_ERROR_INVALID_PARAM;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    int ret = p->network_->join_room(room_id);
    if (ret == PRISM_OK) {
        p->fire_event(PRISM_EVENT_ROOM_JOINED);
    }
    return ret;
}

_API int prism_player_leave_room(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    int ret = p->network_->leave_room();
    if (ret == PRISM_OK) {
        p->fire_event(PRISM_EVENT_ROOM_LEFT);
    }
    return ret;
}

_API int prism_player_get_room_info(PrismPlayerHandle player, PrismRoomInfo* info)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    if (!info)   return PRISM_ERROR_INVALID_PARAM;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    return p->network_->get_room_info(info);
}

_API PrismRoomState prism_player_get_room_state(PrismPlayerHandle player)
{
    if (!player) return PRISM_ROOM_DISCONNECTED;
    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    return p->network_->get_room_state();
}

/* ========== 媒体源 ========== */

_API int prism_player_open(PrismPlayerHandle player, const char* uri)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    if (!uri)    return PRISM_ERROR_INVALID_PARAM;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    p->state_.store(PRISM_STATE_LOADING);
    p->media_uri_ = uri;
    p->sync_sm_->reset();
    p->sync_algo_->reset();
    spdlog::info("[PrismPlayer] opening: {}", uri);

    if (!Prism::Service::init_engines(p)) {
        p->state_.store(PRISM_STATE_ERROR);
        p->sync_sm_->transition(Prism::Business::SyncState::ERROR);
        spdlog::error("[PrismPlayer] open failed: engine init error");
        return PRISM_ERROR_OPEN_FAILED;
    }

    p->state_.store(PRISM_STATE_PAUSED);
    p->sync_sm_->transition(Prism::Business::SyncState::CALIBATING);
    p->fire_event(PRISM_EVENT_MEDIA_LOADED);
    spdlog::info("[PrismPlayer] media loaded: {}", uri);
    return PRISM_OK;
}

_API int prism_player_close(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    if (p->audio_engine_) {
        p->audio_engine_->close();
    }
    if (p->video_engine_) {
        p->video_engine_->close();
    }

    p->engines_initialized_ = false;
    p->media_uri_.clear();
    p->media_info_ = {};
    p->sync_sm_->reset();
    p->sync_algo_->reset();
    p->state_.store(PRISM_STATE_IDLE);

    spdlog::info("[PrismPlayer] closed");
    return PRISM_OK;
}

/* ========== 播放控制 ========== */

_API int prism_player_play(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    PrismState s = p->state_.load();

    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) {
        p->last_error_.store(PRISM_ERROR_NO_MEDIA);
        spdlog::warn("[PrismPlayer] play failed: no media loaded");
        return PRISM_ERROR_NO_MEDIA;
    }

    if (s == PRISM_STATE_PLAYING) return PRISM_OK;

    if (!Prism::Service::init_engines(p)) {
        p->state_.store(PRISM_STATE_ERROR);
        spdlog::error("[PrismPlayer] play failed: engine init error");
        return PRISM_ERROR_UNKNOWN;
    }

    p->cmd_dispatcher_->dispatch_play();
    p->sync_sm_->transition(Prism::Business::SyncState::SYNCHRONIZED);
    p->state_.store(PRISM_STATE_PLAYING);
    spdlog::info("[PrismPlayer] playing");
    return PRISM_OK;
}

_API int prism_player_pause(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    if (p->state_.load() != PRISM_STATE_PLAYING) return PRISM_OK;

    p->cmd_dispatcher_->dispatch_pause();
    p->state_.store(PRISM_STATE_PAUSED);
    spdlog::info("[PrismPlayer] paused");
    return PRISM_OK;
}

_API int prism_player_stop(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    spdlog::info("[PrismPlayer] stopping");

    p->cmd_dispatcher_->dispatch_pause();

    if (p->audio_engine_) {
        p->audio_engine_->close();
    }
    if (p->video_engine_) {
        p->video_engine_->close();
    }

    p->engines_initialized_ = false;
    p->sync_sm_->reset();
    p->sync_algo_->reset();
    p->state_.store(PRISM_STATE_STOPPED);
    spdlog::info("[PrismPlayer] stopped");
    return PRISM_OK;
}

_API int prism_player_seek(PrismPlayerHandle player, int64_t position_ms,
                           PrismSeekMode mode)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    PrismState s = p->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) {
        p->last_error_.store(PRISM_ERROR_NO_MEDIA);
        spdlog::warn("[PrismPlayer] seek failed: no media loaded");
        return PRISM_ERROR_NO_MEDIA;
    }

    if (mode == PRISM_SEEK_ABSOLUTE && position_ms < 0) {
        p->last_error_.store(PRISM_ERROR_INVALID_PARAM);
        spdlog::warn("[PrismPlayer] seek failed: negative position in absolute mode");
        return PRISM_ERROR_INVALID_PARAM;
    }

    int seek_flag = (mode == PRISM_SEEK_ABSOLUTE) ? 0 : 1;
    uint64_t target_pts = static_cast<uint64_t>(position_ms);

    if (mode == PRISM_SEEK_RELATIVE) {
        auto drift = p->sync_algo_->get_drift_info();
        int64_t cur = static_cast<int64_t>(drift.audio_pts);
        target_pts = static_cast<uint64_t>(std::max<int64_t>(0, cur + position_ms));
    }

    p->cmd_dispatcher_->dispatch_seek(target_pts, seek_flag);
    p->sync_algo_->reset();
    p->fire_event(PRISM_EVENT_SEEK_COMPLETED);
    spdlog::info("[PrismPlayer] seek to {}ms", target_pts);
    return PRISM_OK;
}

/* ========== 状态查询 ========== */

_API PrismState prism_player_get_state(PrismPlayerHandle player)
{
    if (!player) return PRISM_STATE_ERROR;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->state_.load();
}

_API int64_t prism_player_get_position(PrismPlayerHandle player)
{
    if (!player) return -1;
    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    PrismState s = p->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) return -1;

    auto drift = p->sync_algo_->get_drift_info();
    return static_cast<int64_t>(drift.audio_pts);
}

_API int64_t prism_player_get_duration(PrismPlayerHandle player)
{
    if (!player) return -1;
    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    PrismState s = p->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) return -1;

    return p->media_info_.duration_ms;
}

/* ========== 音量控制 ========== */

_API int prism_player_set_volume(PrismPlayerHandle player, float volume)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    float clamped = Prism::Service::clamp(volume, 0.0f, 1.0f);
    p->volume_.store(clamped);
    spdlog::debug("[PrismPlayer] volume set to {:.2f}", clamped);
    return PRISM_OK;
}

_API float prism_player_get_volume(PrismPlayerHandle player)
{
    if (!player) return 0.0f;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->volume_.load();
}

_API int prism_player_set_mute(PrismPlayerHandle player, bool mute)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

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

_API bool prism_player_get_mute(PrismPlayerHandle player)
{
    if (!player) return false;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->mute_.load();
}

/* ========== 播放属性 ========== */

_API int prism_player_set_playback_speed(PrismPlayerHandle player, float speed)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    float clamped = Prism::Service::clamp(speed, 0.5f, 2.0f);
    p->speed_.store(clamped);

    p->cmd_dispatcher_->dispatch_speed(clamped);

    spdlog::debug("[PrismPlayer] playback speed set to {:.2f}x", clamped);
    return PRISM_OK;
}

_API float prism_player_get_playback_speed(PrismPlayerHandle player)
{
    if (!player) return 1.0f;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->speed_.load();
}

_API int prism_player_set_loop(PrismPlayerHandle player, bool loop)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    static_cast<Prism::Service::PrismPlayerInternal*>(player)->loop_.store(loop);
    spdlog::debug("[PrismPlayer] loop set to {}", loop);
    return PRISM_OK;
}

_API bool prism_player_get_loop(PrismPlayerHandle player)
{
    if (!player) return false;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->loop_.load();
}

/* ========== 视频窗口 ========== */

_API int prism_player_set_video_window(PrismPlayerHandle player, void* native_window)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    static_cast<Prism::Service::PrismPlayerInternal*>(player)->video_window_.store(native_window);
    spdlog::debug("[PrismPlayer] video window set to {}", native_window);
    return PRISM_OK;
}

/* ========== 媒体信息 ========== */

_API int prism_player_get_media_info(PrismPlayerHandle player, PrismMediaInfo* info)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    if (!info)   return PRISM_ERROR_INVALID_PARAM;

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

_API PrismErrorCode prism_player_get_last_error(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->last_error_.load();
}

_API const char* prism_player_get_version(void)
{
    return "PrismPlayer " STRINGIFY(PRISM_VERSION_MAJOR) "."
           STRINGIFY(PRISM_VERSION_MINOR) "."
           STRINGIFY(PRISM_VERSION_PATCH);
}

} // extern "C"
