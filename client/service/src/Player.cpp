#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

#include "API.h"
#include "PlayerImpl.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <chrono>
#include <spdlog/spdlog.h>

namespace Prism::Service {

/* ========== PrismPlayerInternal 实现 ========== */

static PrismConfig default_config() {
    PrismConfig c{};
    c.log_level = "info";
    return c;
}

int64_t PrismPlayerInternal::system_time_ms()
{
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    return ms.count();
}

PrismPlayerInternal::PrismPlayerInternal(const PrismConfig& cfg,
                                         PrismEventCallback cb,
                                         void* ud)
    : sync_config_()
    , sync_(sync_config_)
    , config_(cfg)
    , log_level_(cfg.log_level ? cfg.log_level : "info")
    , callback_(cb)
    , user_data_(ud)
{
    volume_.store(config_.default_volume);
    spdlog::set_level(spdlog::level::from_str(log_level_));
    spdlog::info("[PrismPlayer] instance created");
}

PrismPlayerInternal::~PrismPlayerInternal()
{
    if (engines_initialized_) {
        if (audio_engine_) audio_engine_->close();
        if (video_engine_) video_engine_->close();
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

static bool init_engines(PrismPlayerInternal* p)
{
    if (p->engines_initialized_) return true;

    if (!p->audio_engine_) {
        p->audio_engine_ = p->audio_factory_.create_audio_engine();
        if (!p->audio_engine_ || !p->audio_engine_->init()) {
            spdlog::error("[PrismPlayer] failed to init audio engine");
            p->audio_engine_.reset();
            p->last_error_.store(PRISM_ERROR_UNKNOWN);
            return false;
        }
    }

    if (!p->video_engine_ && p->config_.enable_video) {
        p->video_engine_ = p->video_factory_.create_audio_engine();
        if (!p->video_engine_ || !p->video_engine_->init()) {
            spdlog::error("[PrismPlayer] failed to init video engine");
            p->video_engine_.reset();
            p->last_error_.store(PRISM_ERROR_UNKNOWN);
            return false;
        }
    }

    p->engines_initialized_ = true;
    return true;
}

/* ========== 同步状态推送到同步器 ========== */

/**
 * @brief 将引擎层的同步信息推送到业务层同步器
 */
static void push_sync_info(PrismPlayerInternal* p)
{
    int64_t now_ms = PrismPlayerInternal::system_time_ms();

    if (p->audio_engine_) {
        auto info = p->audio_engine_->get_sync_info();
        int64_t audio_pts = static_cast<int64_t>(info.current_pts.load());
        if (audio_pts > 0) {
            p->sync_.update_audio_clock(audio_pts, now_ms);
        }
    }

    if (p->video_engine_) {
        auto info = p->video_engine_->get_sync_info();
        int64_t video_pts = static_cast<int64_t>(info.current_pts.load());
        if (video_pts > 0) {
            p->sync_.update_video_clock(video_pts, now_ms);
        }
    }
}

/* ========== 值钳位辅助 ========== */

static float clamp(float val, float lo, float hi)
{
    return std::max(lo, std::min(hi, val));
}

} // namespace Prism::Service

/* ========== C API 实现 ========== */

extern "C" {

_API PrismPlayerHandle prism_player_create(const PrismConfig* config,
                                            PrismEventCallback callback,
                                            void* user_data)
{
    PrismConfig cfg = config ? *config : Prism::Service::default_config();

    auto* p = new (std::nothrow) Prism::Service::PrismPlayerInternal(cfg, callback, user_data);
    if (!p) return nullptr;

    return static_cast<PrismPlayerHandle>(p);
}

_API void prism_player_destroy(PrismPlayerHandle player)
{
    if (!player) return;
    delete static_cast<Prism::Service::PrismPlayerInternal*>(player);
}

_API int prism_player_open(PrismPlayerHandle player, const char* uri)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    if (!uri)  return PRISM_ERROR_INVALID_PARAM;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    p->state_.store(PRISM_STATE_LOADING);
    p->media_uri_ = uri;

    // 重置同步器状态
    p->sync_.reset();

    if (!Prism::Service::init_engines(p)) {
        p->state_.store(PRISM_STATE_ERROR);
        spdlog::error("[PrismPlayer] open failed for uri: {}", uri);
        return PRISM_ERROR_OPEN_FAILED;
    }

    spdlog::info("[PrismPlayer] open: {}", uri);

    p->state_.store(PRISM_STATE_PAUSED);
    p->fire_event(PRISM_EVENT_MEDIA_LOADED);

    return PRISM_OK;
}

_API int prism_player_close(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    if (p->audio_engine_) p->audio_engine_->close();
    if (p->video_engine_) p->video_engine_->close();

    p->sync_.reset();
    p->engines_initialized_ = false;
    p->media_uri_.clear();
    p->state_.store(PRISM_STATE_IDLE);

    spdlog::info("[PrismPlayer] closed");
    return PRISM_OK;
}

_API int prism_player_play(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    PrismState s = p->state_.load();

    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) {
        p->last_error_.store(PRISM_ERROR_NO_MEDIA);
        return PRISM_ERROR_NO_MEDIA;
    }

    if (s == PRISM_STATE_PLAYING) return PRISM_OK;

    if (!Prism::Service::init_engines(p)) {
        p->state_.store(PRISM_STATE_ERROR);
        return PRISM_ERROR_UNKNOWN;
    }

    // 恢复同步器时钟
    int64_t now_ms = Prism::Service::PrismPlayerInternal::system_time_ms();
    p->sync_.resume_clock(now_ms);

    if (p->audio_engine_) p->audio_engine_->play();
    if (p->video_engine_) p->video_engine_->play();

    p->state_.store(PRISM_STATE_PLAYING);
    spdlog::info("[PrismPlayer] playing");
    return PRISM_OK;
}

_API int prism_player_pause(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    if (p->state_.load() != PRISM_STATE_PLAYING) return PRISM_OK;

    // 暂停同步器时钟，冻结当前 PTS
    p->sync_.pause_clock();

    if (p->audio_engine_) p->audio_engine_->pause();
    if (p->video_engine_) p->video_engine_->pause();

    p->state_.store(PRISM_STATE_PAUSED);
    spdlog::info("[PrismPlayer] paused");
    return PRISM_OK;
}

_API int prism_player_stop(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    if (p->audio_engine_) {
        p->audio_engine_->pause();
        p->audio_engine_->close();
    }
    if (p->video_engine_) {
        p->video_engine_->pause();
        p->video_engine_->close();
    }

    p->sync_.reset();
    p->engines_initialized_ = false;
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
        return PRISM_ERROR_NO_MEDIA;
    }

    if (mode == PRISM_SEEK_ABSOLUTE && position_ms < 0) {
        p->last_error_.store(PRISM_ERROR_INVALID_PARAM);
        return PRISM_ERROR_INVALID_PARAM;
    }

    uint64_t target_pts = static_cast<uint64_t>(position_ms);
    if (mode == PRISM_SEEK_RELATIVE) {
        int64_t cur = prism_player_get_position(player);
        target_pts = static_cast<uint64_t>(std::max<int64_t>(0, cur + position_ms));
    }

    int seek_flag = static_cast<int>(mode);

    if (p->audio_engine_) p->audio_engine_->seek(target_pts, seek_flag);
    if (p->video_engine_) p->video_engine_->seek(target_pts, seek_flag);

    // Seek 后同步器以新的目标位置作为基准，重新校准
    int64_t now_ms = Prism::Service::PrismPlayerInternal::system_time_ms();
    p->sync_.update_audio_clock(static_cast<int64_t>(target_pts), now_ms);
    p->sync_.update_video_clock(static_cast<int64_t>(target_pts), now_ms);

    p->fire_event(PRISM_EVENT_SEEK_COMPLETED);
    spdlog::info("[PrismPlayer] seek to {}ms (flag={})", target_pts, seek_flag);
    return PRISM_OK;
}

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

    // 优先推送引擎同步信息到同步器
    Prism::Service::push_sync_info(p);

    // 从业务层同步器获取主时钟 PTS
    int64_t pos = p->sync_.get_master_pts();
    if (pos > 0) return pos;

    // 回退：直接从音频引擎读取
    if (p->audio_engine_) {
        auto info = p->audio_engine_->get_sync_info();
        return static_cast<int64_t>(info.current_pts.load());
    }
    return -1;
}

_API int64_t prism_player_get_duration(PrismPlayerHandle player)
{
    if (!player) return -1;
    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    PrismState s = p->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) return -1;

    return p->media_info_.duration_ms;
}

_API int prism_player_set_volume(PrismPlayerHandle player, float volume)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    float clamped = Prism::Service::clamp(volume, 0.0f, 1.0f);
    p->volume_.store(clamped);

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
    } else if (!mute && was_muted) {
        p->volume_.store(p->volume_before_mute_.load());
    }

    return PRISM_OK;
}

_API bool prism_player_get_mute(PrismPlayerHandle player)
{
    if (!player) return false;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->mute_.load();
}

_API int prism_player_set_playback_speed(PrismPlayerHandle player, float speed)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    float clamped = Prism::Service::clamp(speed, 0.5f, 2.0f);
    p->speed_.store(clamped);

    // 同步速度到引擎和同步器
    if (p->audio_engine_) p->audio_engine_->set_play_speed();
    if (p->video_engine_) p->video_engine_->set_play_speed(clamped);
    p->sync_.set_play_speed(static_cast<double>(clamped));

    spdlog::info("[PrismPlayer] playback speed set to {}x", clamped);
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
    return PRISM_OK;
}

_API bool prism_player_get_loop(PrismPlayerHandle player)
{
    if (!player) return false;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->loop_.load();
}

_API int prism_player_set_video_window(PrismPlayerHandle player, void* native_window)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    static_cast<Prism::Service::PrismPlayerInternal*>(player)->video_window_.store(native_window);
    return PRISM_OK;
}

_API int prism_player_get_media_info(PrismPlayerHandle player, PrismMediaInfo* info)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    if (!info)  return PRISM_ERROR_INVALID_PARAM;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    PrismState s = p->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) {
        p->last_error_.store(PRISM_ERROR_NO_MEDIA);
        return PRISM_ERROR_NO_MEDIA;
    }

    *info = p->media_info_;
    return PRISM_OK;
}

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
