#include "EngineObserver.h"

#include <spdlog/spdlog.h>

namespace Prism::Business {

EngineObserver::EngineObserver() : owned_clock_(std::make_unique<SystemSyncClock>()) {}

void EngineObserver::on_audio_frame(const Prism::Engine::AudioSyncInfo& info) {
    if (paused_) {
        return;
    }

    uint64_t pts = info.current_pts.load();
    last_audio_pts_ = pts;
    if (sync_algo_) {
        sync_algo_->update_audio_pts(pts);
    }
    spdlog::trace("[EngineObserver] audio frame pts={}", pts);

    if (state_machine_ && state_machine_->get_state() == SyncState::UNINIT &&
        state_machine_->transition(SyncState::CALIBATING)) {
        calibration_start_ms_ = now_ms();
        calibration_started_ = true;
    }
}

void EngineObserver::on_video_frame(const Prism::Engine::VideoSyncInfo& info) {
    if (paused_ || !sync_algo_ || !state_machine_) {
        return;
    }

    uint64_t video_pts = info.current_pts.load();
    spdlog::trace("[EngineObserver] video frame pts={}", video_pts);

    SyncState state = state_machine_->get_state();
    if (state != SyncState::CALIBATING && state != SyncState::SYNCHRONIZED &&
        state != SyncState::AHEAD && state != SyncState::BEHIND) {
        return;
    }

    // 始终用真实 PTS 计算 drift，保证偏差可观测；校准期强制 RENDER，不丢帧。
    SyncAction action = sync_algo_->calibrate(last_audio_pts_, video_pts);

    if (state == SyncState::CALIBATING) {
        if (!calibration_started_) {
            calibration_start_ms_ = now_ms();
            calibration_started_ = true;
        }
        last_action_ = SyncAction::RENDER;
        if (calibration_elapsed()) {
            state_machine_->transition(SyncState::SYNCHRONIZED);
            spdlog::debug("[EngineObserver] calibration finished after {}ms",
                          sync_algo_->get_config().calibrate_duration_ms);
        }
        return;
    }

    last_action_ = action;
    switch (action) {
    case SyncAction::RENDER:
        state_machine_->transition(SyncState::SYNCHRONIZED);
        break;
    case SyncAction::WAIT:
        state_machine_->transition(SyncState::AHEAD);
        break;
    case SyncAction::DROP:
        state_machine_->transition(SyncState::BEHIND);
        break;
    }
}

void EngineObserver::on_render_ready(const Prism::Engine::RenderMetadata& metadata) {
    spdlog::trace("[EngineObserver] render ready: {}x{} ts={}", metadata.width, metadata.height,
                  metadata.timestamp);
}

void EngineObserver::set_sync_algorithm(ISyncAlgorithm* algo) {
    sync_algo_ = algo;
}

void EngineObserver::set_state_machine(IPlaybackStateMachine* sm) {
    state_machine_ = sm;
}

void EngineObserver::set_clock(ISyncClock* clock) {
    clock_ = clock;
}

void EngineObserver::set_paused(bool paused) {
    if (paused_ == paused) {
        return;
    }
    paused_ = paused;
    if (paused) {
        pause_start_ms_ = now_ms();
    } else {
        // 把暂停期间经过的墙钟时间补偿出校准窗口，避免恢复后立即退出校准。
        if (calibration_started_) {
            calibration_start_ms_ += now_ms() - pause_start_ms_;
        }
    }
}

void EngineObserver::reset() {
    last_audio_pts_ = 0;
    last_action_ = SyncAction::RENDER;
    paused_ = false;
    calibration_start_ms_ = 0;
    calibration_started_ = false;
    if (sync_algo_) {
        sync_algo_->reset();
    }
}

SyncAction EngineObserver::get_last_action() const {
    return last_action_;
}

uint64_t EngineObserver::now_ms() const {
    return (clock_ ? clock_ : owned_clock_.get())->now_ms();
}

bool EngineObserver::calibration_elapsed() const {
    if (!calibration_started_ || !sync_algo_) {
        return false;
    }
    uint64_t duration = sync_algo_->get_config().calibrate_duration_ms;
    return now_ms() >= calibration_start_ms_ && now_ms() - calibration_start_ms_ >= duration;
}

} // namespace Prism::Business
