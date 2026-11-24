#pragma once

#include "AudioEngine.h"
#include "AudioEngineFactory.h"
#include "VideoEngine.h"
#include "VideoEngineFactory.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>

/**
 * @file FakeEngines.h
 * @brief 验收用的记录型假音频/假视频引擎（#88，模式 A）。
 *
 * 实现 Engine 层**已存在**的 `AudioEngineFactory` / `VideoEngineFactory` 与两个引擎接口，
 * 经 ABI 上已有的 `prism_player_set_audio_factory` / `prism_player_set_video_factory` 注入。
 * 不访问音频设备、GPU 或窗口，因此无设备的机器也能跑（#87 决策 6 / 9）。
 *
 * 用例在 ABI 之外直接读 `EngineRecorder` 的计数，验证“产品真的驱动了引擎”——
 * 这是 #87 明确允许的断言来源（注入的假后端自己记录下来的计数），不读产品私有成员。
 */

namespace Prism::Qa {

/**
 * @struct EngineRecorder
 * @brief 假引擎的共享记录状态。
 *
 * 引擎归产品所有（`CommandDispatcher` 持有 unique_ptr），工厂与用例共享 recorder，
 * 所以用例不需要触碰产品内部就能核对调用次数与参数。
 * 成员都是 atomic：将来引擎被移到工作线程时不会引入数据竞争。
 */
struct EngineRecorder {
    std::atomic<int> create_calls{0}; /**< 工厂被要求创建引擎的次数 */
    std::atomic<int> init_calls{0};   /**< init() 调用次数 */
    std::atomic<int> play_calls{0};   /**< play() 调用次数 */
    std::atomic<int> pause_calls{0};  /**< pause() 调用次数 */
    std::atomic<int> close_calls{0};  /**< close() 调用次数 */
    std::atomic<int> seek_calls{0};   /**< seek() 调用次数 */
    std::atomic<int> speed_calls{0};  /**< set_play_speed() 调用次数 */

    std::atomic<std::int64_t> last_seek_ms{-1}; /**< 最近一次 seek 的绝对位置（毫秒） */
    std::atomic<float> last_speed{1.0F};        /**< 最近一次倍速 */
};

/**
 * @class FakeAudioEngine
 * @brief 记录调用、不接声卡的假音频引擎。
 */
class FakeAudioEngine final : public Prism::Engine::AudioEngine {
  public:
    FakeAudioEngine(std::shared_ptr<EngineRecorder> recorder, bool init_ok);

    bool init() override;
    bool play() override;
    bool pause() override;
    bool close() override;
    bool set_play_speed(float speed) override;
    bool seek(std::chrono::milliseconds pts) override;
    Prism::Engine::AudioSyncInfo get_sync_info() override;

  private:
    std::shared_ptr<EngineRecorder> recorder_;
    bool init_ok_;
};

/**
 * @class FakeVideoEngine
 * @brief 记录调用、不接 GPU / 窗口的假视频引擎。
 */
class FakeVideoEngine final : public Prism::Engine::VideoEngine {
  public:
    FakeVideoEngine(std::shared_ptr<EngineRecorder> recorder, bool init_ok);

    bool init() override;
    bool play() override;
    bool pause() override;
    bool close() override;
    bool set_play_speed(float speed) override;
    bool seek(std::chrono::milliseconds pts) override;
    Prism::Engine::VideoSyncInfo get_sync_info() override;
    Prism::Engine::RenderMetadata get_render_result() override;

  private:
    std::shared_ptr<EngineRecorder> recorder_;
    bool init_ok_;
};

/**
 * @class FakeAudioFactory
 * @brief `AudioEngineFactory` 的验收实现；`init_ok=false` 用于注入初始化失败的引擎。
 */
class FakeAudioFactory final : public Prism::Engine::AudioEngineFactory {
  public:
    FakeAudioFactory();
    explicit FakeAudioFactory(std::shared_ptr<EngineRecorder> recorder, bool init_ok = true);

    std::unique_ptr<Prism::Engine::AudioEngine> create_audio_engine() override;

    [[nodiscard]] const std::shared_ptr<EngineRecorder>& recorder() const { return recorder_; }

  private:
    std::shared_ptr<EngineRecorder> recorder_;
    bool init_ok_;
};

/**
 * @class FakeVideoFactory
 * @brief `VideoEngineFactory` 的验收实现；`init_ok=false` 用于注入初始化失败的引擎。
 */
class FakeVideoFactory final : public Prism::Engine::VideoEngineFactory {
  public:
    FakeVideoFactory();
    explicit FakeVideoFactory(std::shared_ptr<EngineRecorder> recorder, bool init_ok = true);

    std::unique_ptr<Prism::Engine::VideoEngine> create_video_engine() override;

    [[nodiscard]] const std::shared_ptr<EngineRecorder>& recorder() const { return recorder_; }

  private:
    std::shared_ptr<EngineRecorder> recorder_;
    bool init_ok_;
};

} // namespace Prism::Qa
