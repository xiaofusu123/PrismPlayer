#include "av_synchronizer.h"
#include "av_synchronizer_impl.h"

namespace Prism::Business {

/* ========== 构造 / 重置 ========== */

AVSynchronizer::AVSynchronizer(const SyncConfig& config)
    : config_(config)
{
}

void AVSynchronizer::reset()
{
    audio_clock_  = SyncClock{};
    video_clock_  = SyncClock{};
    master_clock_ = SyncClock{};
    play_speed_   = 1.0;
}

void AVSynchronizer::pause_clock()
{
    audio_clock_.paused  = true;
    video_clock_.paused  = true;
    master_clock_.paused = true;
}

void AVSynchronizer::resume_clock(int64_t sys_time_ms)
{
    audio_clock_.last_sys_time_ms  = sys_time_ms;
    video_clock_.last_sys_time_ms  = sys_time_ms;
    master_clock_.last_sys_time_ms = sys_time_ms;
    audio_clock_.paused  = false;
    video_clock_.paused  = false;
    master_clock_.paused = false;
}

/* ========== 时钟更新 ========== */

void AVSynchronizer::update_audio_clock(int64_t pts_ms, int64_t sys_time_ms)
{
    if (pts_ms < 0 || sys_time_ms < 0) return;

    audio_clock_.pts_ms           = pts_ms;
    audio_clock_.last_sys_time_ms = sys_time_ms;
    audio_clock_.paused           = false;

    if (config_.mode == SYNC_MODE_AUDIO_MASTER) {
        master_clock_ = audio_clock_;
    }
}

void AVSynchronizer::update_video_clock(int64_t pts_ms, int64_t sys_time_ms)
{
    if (pts_ms < 0 || sys_time_ms < 0) return;

    video_clock_.pts_ms           = pts_ms;
    video_clock_.last_sys_time_ms = sys_time_ms;
    video_clock_.paused           = false;

    if (config_.mode == SYNC_MODE_VIDEO_MASTER) {
        master_clock_ = video_clock_;
    }
}

/* ========== 私有：主时钟源选择 ========== */

const SyncClock& AVSynchronizer::master_source() const
{
    switch (config_.mode) {
    case SYNC_MODE_VIDEO_MASTER:
        return video_clock_;
    case SYNC_MODE_EXTERNAL_CLOCK:
        return master_clock_;
    case SYNC_MODE_AUDIO_MASTER:
    default:
        return audio_clock_;
    }
}

/* ========== 私有：时钟PTS推算 ========== */

int64_t AVSynchronizer::clock_pts(const SyncClock& clock, int64_t now_ms) const
{
    return impl::compute_clock_pts(clock, now_ms, play_speed_);
}

/* ========== 同步查询 ========== */

int64_t AVSynchronizer::get_master_pts() const
{
    const SyncClock& src = master_source();
    // 外部时钟模式使用系统时间作为基准
    if (config_.mode == SYNC_MODE_EXTERNAL_CLOCK) {
        return master_clock_.pts_ms;
    }
    return src.pts_ms;
}

SyncResult AVSynchronizer::get_sync_result() const
{
    SyncResult result;
    result.audio_pts_ms  = audio_clock_.pts_ms;
    result.video_pts_ms  = video_clock_.pts_ms;
    result.master_pts_ms = get_master_pts();

    int64_t diff = result.video_pts_ms - result.audio_pts_ms;
    result.sync_drift_ms = static_cast<double>(diff);
    result.is_synced = std::abs(diff) <= config_.sync_threshold_ms;

    return result;
}

int64_t AVSynchronizer::calc_video_delay(int64_t video_pts_ms) const
{
    if (config_.mode != SYNC_MODE_AUDIO_MASTER) {
        return 0;
    }
    if (audio_clock_.last_sys_time_ms == 0) {
        return 0;
    }

    int64_t audio_now = audio_clock_.pts_ms;
    int64_t diff = video_pts_ms - audio_now;

    if (std::abs(diff) <= config_.sync_threshold_ms) {
        return 0;
    }

    if (diff > 0) {
        return std::min(diff, config_.max_drift_ms);
    }

    if (impl::should_drop_frame(diff, config_.frame_drop_threshold_ms)) {
        return diff;
    }

    return 0;
}

/* ========== 控制 ========== */

void AVSynchronizer::set_mode(SyncMode mode)
{
    if (config_.mode == mode) return;

    config_.mode = mode;

    switch (mode) {
    case SYNC_MODE_AUDIO_MASTER:
        master_clock_ = audio_clock_;
        break;
    case SYNC_MODE_VIDEO_MASTER:
        master_clock_ = video_clock_;
        break;
    case SYNC_MODE_EXTERNAL_CLOCK:
        master_clock_.pts_ms           = audio_clock_.pts_ms;
        master_clock_.last_sys_time_ms = 0;
        break;
    }
}

} // namespace Prism::Business
