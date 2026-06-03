#pragma once

#include "IPlaybackStateMachine.h"
#include "ISyncAlgorithm.h"
#include "ICommandDispatcher.h"
#include "IEngineObserver.h"

#include <memory>

namespace Prism::Business {

/**
 * @brief 创建播放状态机实例
 */
std::unique_ptr<IPlaybackStateMachine> create_playback_state_machine();

/**
 * @brief 创建同步算法实例
 */
std::unique_ptr<ISyncAlgorithm> create_sync_algorithm();

/**
 * @brief 创建指令分发器实例
 */
std::unique_ptr<ICommandDispatcher> create_command_dispatcher();

/**
 * @brief 创建引擎观察者实例
 */
std::unique_ptr<IEngineObserver> create_engine_observer();

} // namespace Prism::Business
