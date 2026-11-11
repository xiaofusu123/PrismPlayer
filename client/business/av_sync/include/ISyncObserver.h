#pragma once

#include "IEngineObserver.h"

namespace Prism::Business {

class ISyncAlgorithm;
class IPlaybackStateMachine;

/**
 * @class ISyncObserver
 * @brief 同步观察者接口
 *
 * 继承引擎层对外声明的帧回调接口（Prism::Engine::IEngineObserver），
 * 并额外提供同步算法与状态机的装配入口，供 Service 层在装配时调用。
 */
class ISyncObserver : public Prism::Engine::IEngineObserver {
  public:
    ~ISyncObserver() override = default;

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
};

} // namespace Prism::Business
