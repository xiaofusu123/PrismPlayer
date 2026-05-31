#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

#include "PlayerImpl.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <spdlog/spdlog.h>

namespace Prism::Service {

/* ========== PrismPlayerInternal 实现 ========== */

static PrismConfig default_config() {
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

PrismPlayerInternal::PrismPlayerInternal(const PrismConfig& cfg,
                                         PrismEventCallback cb,
                                         void* ud)
    : config_(cfg)
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
        p->video_engine_ = p->video_factory_.create_audio_engine(); // VideoEngineFactory bug: method named create_audio_engine
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

/* ========== 值钳位辅助 ========== */

static float clamp(float val, float lo, float hi)
{
    return std::max(lo, std::min(hi, val));
}

} // namespace Prism::Service

/* ========== C API 实现 ========== */

extern "C" {

PrismPlayerHandle prism_player_create(const PrismConfig* config,
                                      PrismEventCallback callback,
                                      void* user_data)
{
    PrismConfig cfg = config ? *config : Prism::Service::default_config();

    auto* p = new (std::nothrow) Prism::Service::PrismPlayerInternal(cfg, callback, user_data);
    if (!p) return nullptr;

    return static_cast<PrismPlayerHandle>(p);
}

void prism_player_destroy(PrismPlayerHandle player)
{
    if (!player) return;
    delete static_cast<Prism::Service::PrismPlayerInternal*>(player);
}

int prism_player_open(PrismPlayerHandle player, const char* uri)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    if (!uri)  return PRISM_ERROR_INVALID_PARAM;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    p->state_.store(PRISM_STATE_LOADING);
    p->media_uri_ = uri;

    if (!Prism::Service::init_engines(p)) {
        p->state_.store(PRISM_STATE_ERROR);
        return PRISM_ERROR_OPEN_FAILED;
    }

    spdlog::info("[PrismPlayer] open: {}", uri);

    // 媒体加载完成后触发事件（engine 层实现后由实际解码流程触发）
    p->state_.store(PRISM_STATE_PAUSED);
    p->fire_event(PRISM_EVENT_MEDIA_LOADED);

    return PRISM_OK;
}

int prism_player_close(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    if (p->audio_engine_) p->audio_engine_->close();
    if (p->video_engine_) p->video_engine_->close();

    p->engines_initialized_ = false;
    p->media_uri_.clear();
    p->state_.store(PRISM_STATE_IDLE);

    spdlog::info("[PrismPlayer] closed");
    return PRISM_OK;
}

int prism_player_play(PrismPlayerHandle player)
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

    if (p->audio_engine_) p->audio_engine_->play();
    if (p->video_engine_) p->video_engine_->play();

    p->state_.store(PRISM_STATE_PLAYING);
    spdlog::info("[PrismPlayer] playing");
    return PRISM_OK;
}

int prism_player_pause(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    if (p->state_.load() != PRISM_STATE_PLAYING) return PRISM_OK;

    if (p->audio_engine_) p->audio_engine_->pause();
    if (p->video_engine_) p->video_engine_->pause();

    p->state_.store(PRISM_STATE_PAUSED);
    spdlog::info("[PrismPlayer] paused");
    return PRISM_OK;
}

int prism_player_stop(PrismPlayerHandle player)
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

    p->engines_initialized_ = false;
    p->state_.store(PRISM_STATE_STOPPED);
    spdlog::info("[PrismPlayer] stopped");
    return PRISM_OK;
}

int prism_player_seek(PrismPlayerHandle player, int64_t position_ms,
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

    int seek_flag;
    if (mode == PRISM_SEEK_ABSOLUTE) {
        seek_flag = 0;
    } else if (mode == PRISM_SEEK_RELATIVE) {
        seek_flag = 1;
    } else {
        p->last_error_.store(PRISM_ERROR_INVALID_PARAM);
        return PRISM_ERROR_INVALID_PARAM;
    }

    if (p->audio_engine_) p->audio_engine_->seek(target_pts, seek_flag);
    if (p->video_engine_) p->video_engine_->seek(target_pts, seek_flag);

    p->fire_event(PRISM_EVENT_SEEK_COMPLETED);
    spdlog::info("[PrismPlayer] seek to {}ms", target_pts);
    return PRISM_OK;
}

PrismState prism_player_get_state(PrismPlayerHandle player)
{
    if (!player) return PRISM_STATE_ERROR;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->state_.load();
}

int64_t prism_player_get_position(PrismPlayerHandle player)
{
    if (!player) return -1;
    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    PrismState s = p->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) return -1;

    if (p->audio_engine_) {
        auto info = p->audio_engine_->get_sync_info();
        return static_cast<int64_t>(info.current_pts.load());
    }
    return -1;
}

int64_t prism_player_get_duration(PrismPlayerHandle player)
{
    if (!player) return -1;
    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);

    PrismState s = p->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) return -1;

    return p->media_info_.duration_ms;
}

int prism_player_set_volume(PrismPlayerHandle player, float volume)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    float clamped = Prism::Service::clamp(volume, 0.0f, 1.0f);
    p->volume_.store(clamped);

    return PRISM_OK;
}

float prism_player_get_volume(PrismPlayerHandle player)
{
    if (!player) return 0.0f;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->volume_.load();
}

int prism_player_set_mute(PrismPlayerHandle player, bool mute)
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

bool prism_player_get_mute(PrismPlayerHandle player)
{
    if (!player) return false;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->mute_.load();
}

int prism_player_set_playback_speed(PrismPlayerHandle player, float speed)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;

    auto* p = static_cast<Prism::Service::PrismPlayerInternal*>(player);
    float clamped = Prism::Service::clamp(speed, 0.5f, 2.0f);
    p->speed_.store(clamped);

    if (p->audio_engine_) p->audio_engine_->set_play_speed();
    if (p->video_engine_) p->video_engine_->set_play_speed(clamped);

    return PRISM_OK;
}

float prism_player_get_playback_speed(PrismPlayerHandle player)
{
    if (!player) return 1.0f;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->speed_.load();
}

int prism_player_set_loop(PrismPlayerHandle player, bool loop)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    static_cast<Prism::Service::PrismPlayerInternal*>(player)->loop_.store(loop);
    return PRISM_OK;
}

bool prism_player_get_loop(PrismPlayerHandle player)
{
    if (!player) return false;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->loop_.load();
}

int prism_player_set_video_window(PrismPlayerHandle player, void* native_window)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    static_cast<Prism::Service::PrismPlayerInternal*>(player)->video_window_.store(native_window);
    return PRISM_OK;
}

int prism_player_get_media_info(PrismPlayerHandle player, PrismMediaInfo* info)
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

PrismErrorCode prism_player_get_last_error(PrismPlayerHandle player)
{
    if (!player) return PRISM_ERROR_INVALID_HANDLE;
    return static_cast<Prism::Service::PrismPlayerInternal*>(player)->last_error_.load();
}

const char* prism_player_get_version(void)
{
    return "PrismPlayer " STRINGIFY(PRISM_VERSION_MAJOR) "."
           STRINGIFY(PRISM_VERSION_MINOR) "."
           STRINGIFY(PRISM_VERSION_PATCH);
}

} // extern "C"
