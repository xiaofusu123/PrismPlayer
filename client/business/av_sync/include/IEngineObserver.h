#pragma once

#include "EngineObserver.h"

namespace Prism::Business {

class ISyncAlgorithm;
class IPlaybackStateMachine;

/**
 * @class IEngineObserver
 * @brief AV Sync 侧引擎帧回调观察者抽象接口
 *
 * 继承引擎层声明的 Prism::Engine::EngineObserver（三个帧回调），
 * 并额外提供同步算法/状态机的装配方法。引擎只依赖 Engine::EngineObserver，
 * 不依赖本类，从而避免引擎层对 Business 层的反向依赖。
 */
class IEngineObserver : public Prism::Engine::EngineObserver {
public:
    ~IEngineObserver() override = default;

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
