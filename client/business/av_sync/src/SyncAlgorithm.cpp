#include "SyncAlgorithm.h"

#include <spdlog/spdlog.h>

namespace Prism::Business {

SyncAction SyncAlgorithm::calibrate(uint64_t audio_pts, uint64_t video_pts) {
    audio_pts_.store(audio_pts);
    video_pts_.store(video_pts);

    if (audio_pts == 0 || video_pts == 0) {
        drift_ms_.store(0);
        return SyncAction::RENDER;
    }

    int64_t drift = static_cast<int64_t>(video_pts) - static_cast<int64_t>(audio_pts);
    drift_ms_.store(drift);

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

void SyncAlgorithm::configure(const SyncConfig& config) {
    config_ = config;
    spdlog::debug("[SyncAlgorithm] configured: ahead={}ms behind={}ms calibrate={}ms",
                  config_.ahead_threshold_ms, config_.behind_threshold_ms,
                  config_.calibrate_duration_ms);
}

SyncConfig SyncAlgorithm::get_config() const {
    return config_;
}

void SyncAlgorithm::update_audio_pts(uint64_t audio_pts) {
    audio_pts_.store(audio_pts);
}

DriftInfo SyncAlgorithm::get_drift_info() const {
    DriftInfo info;
    info.drift_ms = drift_ms_.load();
    info.audio_pts = audio_pts_.load();
    info.video_pts = video_pts_.load();
    return info;
}

void SyncAlgorithm::reset() {
    drift_ms_.store(0);
    audio_pts_.store(0);
    video_pts_.store(0);
    spdlog::debug("[SyncAlgorithm] reset");
}

} // namespace Prism::Business
