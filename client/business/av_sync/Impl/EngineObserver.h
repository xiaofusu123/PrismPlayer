#pragma once

#include "IEngineObserver.h"
#include "ISyncAlgorithm.h"
#include "IPlaybackStateMachine.h"

namespace Prism::Business {

/**
 * @class EngineObserver
 * @brief 引擎观察者实现，接收帧回调并驱动同步校准
 */
class EngineObserver : public IEngineObserver {
public:
    EngineObserver();
    ~EngineObserver() override = default;

    void on_audio_frame(const Prism::Engine::AudioSyncInfo& info) override;
    void on_video_frame(const Prism::Engine::VideoSyncInfo& info) override;
    void on_render_ready(const Prism::Engine::RenderMetadata& metadata) override;
    void set_sync_algorithm(ISyncAlgorithm* algo) override;
    void set_state_machine(IPlaybackStateMachine* sm) override;

private:
    ISyncAlgorithm* sync_algo_{nullptr};
    IPlaybackStateMachine* state_machine_{nullptr};
    uint64_t last_audio_pts_{0};
};

} // namespace Prism::Business
