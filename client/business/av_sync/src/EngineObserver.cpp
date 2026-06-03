#include "../Impl/EngineObserver.h"

#include <spdlog/spdlog.h>

namespace Prism::Business {

EngineObserver::EngineObserver() = default;

void EngineObserver::on_audio_frame(const Prism::Engine::AudioSyncInfo& info)
{
    uint64_t pts = info.current_pts.load();
    last_audio_pts_ = pts;
    spdlog::trace("[EngineObserver] audio frame pts={}", pts);

    if (state_machine_) {
        SyncState s = state_machine_->get_state();
        if (s == SyncState::UNINIT) {
            state_machine_->transition(SyncState::CALIBATING);
        }
    }
}

void EngineObserver::on_video_frame(const Prism::Engine::VideoSyncInfo& info)
{
    uint64_t video_pts = info.current_pts.load();
    spdlog::trace("[EngineObserver] video frame pts={}", video_pts);

    if (sync_algo_ && state_machine_) {
        SyncState s = state_machine_->get_state();
        if (s == SyncState::CALIBATING || s == SyncState::SYNCHRONIZED ||
            s == SyncState::AHEAD || s == SyncState::BEHIND) {

            SyncAction action = sync_algo_->calibrate(last_audio_pts_, video_pts);

            if (s == SyncState::CALIBATING) {
                // First sync point: always route through SYNCHRONIZED per state machine design
                state_machine_->transition(SyncState::SYNCHRONIZED);
            } else {
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
        }
    }
}

void EngineObserver::on_render_ready(const Prism::Engine::RenderMetadata& metadata)
{
    spdlog::trace("[EngineObserver] render ready: {}x{} ts={}",
                  metadata.width, metadata.height, metadata.timestamp);
}

void EngineObserver::set_sync_algorithm(ISyncAlgorithm* algo)
{
    sync_algo_ = algo;
}

void EngineObserver::set_state_machine(IPlaybackStateMachine* sm)
{
    state_machine_ = sm;
}

} // namespace Prism::Business
