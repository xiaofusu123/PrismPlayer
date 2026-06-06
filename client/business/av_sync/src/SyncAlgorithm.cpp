#include "SyncAlgorithm.h"

#include <spdlog/spdlog.h>

namespace Prism::Business {

SyncAlgorithm::SyncAlgorithm()
{
    drift_info_.drift_ms  = 0;
    drift_info_.audio_pts = 0;
    drift_info_.video_pts = 0;
}

SyncAction SyncAlgorithm::calibrate(uint64_t audio_pts, uint64_t video_pts)
{
    drift_info_.audio_pts = audio_pts;
    drift_info_.video_pts = video_pts;

    if (audio_pts == 0 || video_pts == 0) {
        return SyncAction::RENDER;
    }

    int64_t drift = static_cast<int64_t>(video_pts) - static_cast<int64_t>(audio_pts);
    drift_info_.drift_ms = drift;

    if (drift > static_cast<int64_t>(config_.ahead_threshold_ms)) {
        spdlog::trace("[SyncAlgorithm] video ahead by {}ms -> WAIT", drift);
        return SyncAction::WAIT;
    }

    if (drift < -static_cast<int64_t>(config_.behind_threshold_ms)) {
        spdlog::trace("[SyncAlgorithm] video behind by {}ms -> DROP", -drift);
        return SyncAction::DROP;
    }

    return SyncAction::RENDER;
}

void SyncAlgorithm::configure(const SyncConfig& config)
{
    config_ = config;
    spdlog::debug("[SyncAlgorithm] configured: ahead={}ms behind={}ms calibrate={}ms",
                  config_.ahead_threshold_ms, config_.behind_threshold_ms,
                  config_.calibrate_duration_ms);
}

DriftInfo SyncAlgorithm::get_drift_info() const
{
    return drift_info_;
}

void SyncAlgorithm::reset()
{
    drift_info_.drift_ms  = 0;
    drift_info_.audio_pts = 0;
    drift_info_.video_pts = 0;
    spdlog::debug("[SyncAlgorithm] reset");
}

} // namespace Prism::Business
