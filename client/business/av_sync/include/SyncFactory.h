#pragma once

#include "ICommandDispatcher.h"
#include "IEngineObserver.h"
#include "IPlaybackStateMachine.h"
#include "ISyncAlgorithm.h"
#include "ISyncClock.h"

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

/**
 * @brief 创建系统时钟实例（基于 steady_clock）
 */
std::unique_ptr<ISyncClock> create_sync_clock();

} // namespace Prism::Business
