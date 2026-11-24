#pragma once

#include "IEngineObserver.h"
#include "IPlaybackStateMachine.h"
#include "ISyncAlgorithm.h"
#include "ISyncClock.h"
#include "SystemSyncClock.h"

#include <atomic>
#include <memory>

namespace Prism::Business {

/**
 * @class EngineObserver
 * @brief 引擎观察者实现，接收帧回调并驱动同步校准
 *
 * 校准阶段（CALIBATING）按 SyncConfig::calibrate_duration_ms 退出：
 * 时钟由注入的 ISyncClock 提供（生产为 SystemSyncClock，用例注入虚拟时钟），
 * 暂停期间冻结逻辑时间，故暂停不会因墙钟积累漂移或提前结束校准。
 *
 * 帧回调为热路径：本类所有回调只写原子量 / 普通标量，不分配、不阻塞、
 * 不反向调用引擎；注册与注销由 CommandDispatcher 在非回调路径完成。
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
    void set_clock(ISyncClock* clock) override;
    void set_paused(bool paused) override;
    void reset() override;
    [[nodiscard]] SyncAction get_last_action() const override;

  private:
    [[nodiscard]] uint64_t now_ms() const;
    [[nodiscard]] bool calibration_elapsed() const;

    ISyncAlgorithm* sync_algo_{nullptr};
    IPlaybackStateMachine* state_machine_{nullptr};
    ISyncClock* clock_{nullptr};
    std::unique_ptr<ISyncClock> owned_clock_;

    std::atomic<uint64_t> last_audio_pts_{0};
    std::atomic<SyncAction> last_action_{SyncAction::RENDER};
    std::atomic<bool> paused_{false};
    uint64_t calibration_start_ms_{0};
    uint64_t pause_start_ms_{0};
    bool calibration_started_{false};
};

} // namespace Prism::Business
