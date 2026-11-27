#pragma once

namespace Prism::Business {

class IEngineObserver;

/**
 * @class IEngineObserverSink
 * @brief 引擎侧「可接收帧观察者注册」的可选能力接口
 *
 * 设计意图（对应 #129）：
 *   - 帧回调契约由引擎层对外声明，同步侧（Business）的 IEngineObserver 实现它；
 *   - 引擎不依赖 Business：本接口只被装配层（Business 的 CommandDispatcher）向下查询，
 *     产品引擎可以完全不实现它，从而不产生任何对 Business 的反向依赖；
 *   - 测试假引擎实现本接口，即可把 PTS 经 IEngineObserver 推入真实同步算法，
 *     让 drift 从恒 0 变成真实差值。
 *
 * 热路径约束（写在接口上，供实现者与用例共同遵守）：
 *   - 注册 / 注销只在引擎初始化与关闭阶段发生，不在帧回调内执行；
 *   - 注册后，实现者应在每一帧就绪时调用 IEngineObserver 的对应回调；
 *   - 回调运行在引擎的解码 / 渲染线程：只传值、不分配、不阻塞、不反向调用引擎；
 *   - 注销后不得再产生回调；反复注册 / 注销以最后一次为准，不持有所有权。
 */
class IEngineObserverSink {
  public:
    virtual ~IEngineObserverSink() = default;

    /**
     * @brief 注册帧观察者
     * @param observer 观察者指针（不持有所有权）；nullptr 表示注销
     */
    virtual void set_frame_observer(IEngineObserver* observer) = 0;
};

} // namespace Prism::Business
