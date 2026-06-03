#define NOMINMAX
#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

#include "Player.h"
#include "PlayerImpl.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <chrono>
#include <spdlog/spdlog.h>

namespace Prism::Service {

/* ========== 辅助函数 ========== */

static PrismConfig default_config()
{
    PrismConfig c{};
    c.log_level = "info";
    return c;
}

static float clamp(float val, float lo, float hi)
{
    return std::max(lo, std::min(hi, val));
}

/* ========== PrismPlayer::Impl ========== */

int64_t PrismPlayer::Impl::system_time_ms()
{
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    return ms.count();
}

PrismPlayer::Impl::Impl(const PrismConfig& cfg,
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

PrismPlayer::Impl::~Impl()
{
    if (engines_initialized_) {
        if (audio_engine_) audio_engine_->close();
        if (video_engine_) video_engine_->close();
    }
    spdlog::info("[PrismPlayer] instance destroyed");
}

void PrismPlayer::Impl::fire_event(PrismEventType type, const void* data) const
{
    if (callback_) {
        callback_(type, data, user_data_);
    }
}

/* ========== 引擎初始化辅助 ========== */

static bool init_engines(PrismPlayer::Impl* p)
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

static void push_sync_info(PrismPlayer::Impl* p)
{
    int64_t now_ms = PrismPlayer::Impl::system_time_ms();

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

/* ========== PrismPlayer 公开方法 ========== */

PrismPlayer::PrismPlayer(const PrismConfig& config)
    : impl_(std::make_unique<Impl>(config, nullptr, nullptr))
{
}

PrismPlayer::~PrismPlayer() = default;

void PrismPlayer::set_event_callback(PrismEventCallback callback, void* user_data)
{
    impl_->callback_ = callback;
    impl_->user_data_ = user_data;
}

int PrismPlayer::open(const char* uri)
{
    if (!uri) return PRISM_ERROR_INVALID_PARAM;

    impl_->state_.store(PRISM_STATE_LOADING);
    impl_->media_uri_ = uri;

    // 重置同步器状态
    impl_->sync_.reset();

    if (!init_engines(impl_.get())) {
        impl_->state_.store(PRISM_STATE_ERROR);
        spdlog::error("[PrismPlayer] open failed for uri: {}", uri);
        return PRISM_ERROR_OPEN_FAILED;
    }

    spdlog::info("[PrismPlayer] open: {}", uri);

    impl_->state_.store(PRISM_STATE_PAUSED);
    impl_->fire_event(PRISM_EVENT_MEDIA_LOADED);

    return PRISM_OK;
}

int PrismPlayer::close()
{
    if (impl_->audio_engine_) impl_->audio_engine_->close();
    if (impl_->video_engine_) impl_->video_engine_->close();

    impl_->sync_.reset();
    impl_->engines_initialized_ = false;
    impl_->media_uri_.clear();
    impl_->state_.store(PRISM_STATE_IDLE);

    spdlog::info("[PrismPlayer] closed");
    return PRISM_OK;
}

int PrismPlayer::play()
{
    PrismState s = impl_->state_.load();

    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) {
        impl_->last_error_.store(PRISM_ERROR_NO_MEDIA);
        return PRISM_ERROR_NO_MEDIA;
    }

    if (s == PRISM_STATE_PLAYING) return PRISM_OK;

    if (!init_engines(impl_.get())) {
        impl_->state_.store(PRISM_STATE_ERROR);
        return PRISM_ERROR_UNKNOWN;
    }

    // 恢复同步器时钟
    int64_t now_ms = Impl::system_time_ms();
    impl_->sync_.resume_clock(now_ms);

    if (impl_->audio_engine_) impl_->audio_engine_->play();
    if (impl_->video_engine_) impl_->video_engine_->play();

    impl_->state_.store(PRISM_STATE_PLAYING);
    spdlog::info("[PrismPlayer] playing");
    return PRISM_OK;
}

int PrismPlayer::pause()
{
    if (impl_->state_.load() != PRISM_STATE_PLAYING) return PRISM_OK;

    // 暂停同步器时钟，冻结当前 PTS
    impl_->sync_.pause_clock();

    if (impl_->audio_engine_) impl_->audio_engine_->pause();
    if (impl_->video_engine_) impl_->video_engine_->pause();

    impl_->state_.store(PRISM_STATE_PAUSED);
    spdlog::info("[PrismPlayer] paused");
    return PRISM_OK;
}

int PrismPlayer::stop()
{
    if (impl_->audio_engine_) {
        impl_->audio_engine_->pause();
        impl_->audio_engine_->close();
    }
    if (impl_->video_engine_) {
        impl_->video_engine_->pause();
        impl_->video_engine_->close();
    }

    impl_->sync_.reset();
    impl_->engines_initialized_ = false;
    impl_->state_.store(PRISM_STATE_STOPPED);
    spdlog::info("[PrismPlayer] stopped");
    return PRISM_OK;
}

int PrismPlayer::seek(int64_t position_ms, PrismSeekMode mode)
{
    PrismState s = impl_->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) {
        impl_->last_error_.store(PRISM_ERROR_NO_MEDIA);
        return PRISM_ERROR_NO_MEDIA;
    }

    if (mode == PRISM_SEEK_ABSOLUTE && position_ms < 0) {
        impl_->last_error_.store(PRISM_ERROR_INVALID_PARAM);
        return PRISM_ERROR_INVALID_PARAM;
    }

    uint64_t target_pts = static_cast<uint64_t>(position_ms);
    if (mode == PRISM_SEEK_RELATIVE) {
        int64_t cur = get_position();
        target_pts = static_cast<uint64_t>(std::max<int64_t>(0, cur + position_ms));
    }

    int seek_flag = static_cast<int>(mode);

    if (impl_->audio_engine_) impl_->audio_engine_->seek(target_pts, seek_flag);
    if (impl_->video_engine_) impl_->video_engine_->seek(target_pts, seek_flag);

    // Seek 后同步器以新的目标位置作为基准，重新校准
    int64_t now_ms = Impl::system_time_ms();
    impl_->sync_.update_audio_clock(static_cast<int64_t>(target_pts), now_ms);
    impl_->sync_.update_video_clock(static_cast<int64_t>(target_pts), now_ms);

    impl_->fire_event(PRISM_EVENT_SEEK_COMPLETED);
    spdlog::info("[PrismPlayer] seek to {}ms (flag={})", target_pts, seek_flag);
    return PRISM_OK;
}

PrismState PrismPlayer::get_state() const
{
    return impl_->state_.load();
}

int64_t PrismPlayer::get_position() const
{
    PrismState s = impl_->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) return -1;

    // 优先推送引擎同步信息到同步器
    push_sync_info(impl_.get());

    // 从业务层同步器获取主时钟 PTS
    int64_t pos = impl_->sync_.get_master_pts();
    if (pos > 0) return pos;

    // 回退：直接从音频引擎读取
    if (impl_->audio_engine_) {
        auto info = impl_->audio_engine_->get_sync_info();
        return static_cast<int64_t>(info.current_pts.load());
    }
    return -1;
}

int64_t PrismPlayer::get_duration() const
{
    PrismState s = impl_->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) return -1;

    return impl_->media_info_.duration_ms;
}

int PrismPlayer::set_volume(float volume)
{
    float clamped_v = clamp(volume, 0.0f, 1.0f);
    impl_->volume_.store(clamped_v);

    return PRISM_OK;
}

float PrismPlayer::get_volume() const
{
    return impl_->volume_.load();
}

int PrismPlayer::set_mute(bool mute)
{
    bool was_muted = impl_->mute_.exchange(mute);
    if (mute && !was_muted) {
        impl_->volume_before_mute_.store(impl_->volume_.load());
    } else if (!mute && was_muted) {
        impl_->volume_.store(impl_->volume_before_mute_.load());
    }

    return PRISM_OK;
}

bool PrismPlayer::get_mute() const
{
    return impl_->mute_.load();
}

int PrismPlayer::set_playback_speed(float speed)
{
    float clamped_v = clamp(speed, 0.5f, 2.0f);
    impl_->speed_.store(clamped_v);

    // 同步速度到引擎和同步器
    if (impl_->audio_engine_) impl_->audio_engine_->set_play_speed();
    if (impl_->video_engine_) impl_->video_engine_->set_play_speed(clamped_v);
    impl_->sync_.set_play_speed(static_cast<double>(clamped_v));

    spdlog::info("[PrismPlayer] playback speed set to {}x", clamped_v);
    return PRISM_OK;
}

float PrismPlayer::get_playback_speed() const
{
    return impl_->speed_.load();
}

int PrismPlayer::set_loop(bool loop)
{
    impl_->loop_.store(loop);
    return PRISM_OK;
}

bool PrismPlayer::get_loop() const
{
    return impl_->loop_.load();
}

int PrismPlayer::set_video_window(void* native_window)
{
    impl_->video_window_.store(native_window);
    return PRISM_OK;
}

int PrismPlayer::get_media_info(PrismMediaInfo* info) const
{
    if (!info) return PRISM_ERROR_INVALID_PARAM;

    PrismState s = impl_->state_.load();
    if (s == PRISM_STATE_IDLE || s == PRISM_STATE_ERROR) {
        impl_->last_error_.store(PRISM_ERROR_NO_MEDIA);
        return PRISM_ERROR_NO_MEDIA;
    }

    *info = impl_->media_info_;
    return PRISM_OK;
}

PrismErrorCode PrismPlayer::get_last_error() const
{
    return impl_->last_error_.load();
}

const char* PrismPlayer::get_version()
{
    return "PrismPlayer " STRINGIFY(PRISM_VERSION_MAJOR) "."
           STRINGIFY(PRISM_VERSION_MINOR) "."
           STRINGIFY(PRISM_VERSION_PATCH);
}

} // namespace Prism::Service
