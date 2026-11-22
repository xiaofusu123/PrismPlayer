#pragma once

#include "AudioEngine.h"
#include "ISyncClock.h"
#include "SyncTypes.h"
#include "VideoEngine.h"

namespace Prism::Business {

class ISyncAlgorithm;
class IPlaybackStateMachine;

/**
 * @class IEngineObserver
 * @brief 引擎帧回调观察者抽象接口（同步侧实现）
 *
 * 接收 Engine 层的帧回调，驱动同步算法校准。
 * 在帧到达时触发 calibrate，根据结果调整渲染行为。
 *
 * 线程语义与重入约束（#129）：
 *   - 回调发生在引擎的解码 / 渲染线程，属于热路径；
 *   - 实现不得在回调内分配内存、加锁阻塞，也不得反向调用引擎的任何方法
 *     （禁止从回调内再次调用 AudioEngine / VideoEngine，以免重入）；
 *   - 回调可能与 play / pause / seek / close 并发，实现须保证数据竞争安全；
 *   - 注册 / 注销（见 IEngineObserverSink）不在帧回调内执行，避免修改回调集合。
 */
class IEngineObserver {
  public:
    virtual ~IEngineObserver() = default;

    /**
     * @brief 音频帧到达回调
     * @param info 音频同步信息
     */
    virtual void on_audio_frame(const Prism::Engine::AudioSyncInfo& info) = 0;

    /**
     * @brief 视频帧到达回调
     * @param info 视频同步信息
     */
    virtual void on_video_frame(const Prism::Engine::VideoSyncInfo& info) = 0;

    /**
     * @brief 渲染就绪回调
     * @param metadata 渲染元数据
     */
    virtual void on_render_ready(const Prism::Engine::RenderMetadata& metadata) = 0;

    /**
     * @brief 绑定同步算法
     * @param algo 同步算法指针（不持有所有权）
     */
    virtual void set_sync_algorithm(ISyncAlgorithm* algo) = 0;

    /**
     * @brief 绑定状态机
     * @param sm 状态机指针（不持有所有权）
     */
    virtual void set_state_machine(IPlaybackStateMachine* sm) = 0;

    /**
     * @brief 注入同步时钟（不持有所有权）；nullptr 表示使用内置系统时钟
     * @param clock 时钟指针
     */
    virtual void set_clock(ISyncClock* clock) = 0;

    /**
     * @brief 通知暂停 / 恢复；暂停期间冻结漂移与校准计时，不推进播放位置
     * @param paused true=暂停，false=恢复
     */
    virtual void set_paused(bool paused) = 0;

    /**
     * @brief 重置同步观察者状态（seek 后调用，不拿旧时间戳继续计算）
     */
    virtual void reset() = 0;

    /**
     * @brief 取最近一次同步决策（决策闭环可观测面）
     */
    [[nodiscard]] virtual SyncAction get_last_action() const = 0;
};

} // namespace Prism::Business
