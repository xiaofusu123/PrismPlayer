#include "../include/SyncFactory.h"

#include "../Impl/PlaybackStateMachine.h"
#include "../Impl/SyncAlgorithm.h"
#include "../Impl/CommandDispatcher.h"
#include "../Impl/EngineObserver.h"

namespace Prism::Business {

std::unique_ptr<IPlaybackStateMachine> create_playback_state_machine()
{
    return std::make_unique<PlaybackStateMachine>();
}

std::unique_ptr<ISyncAlgorithm> create_sync_algorithm()
{
    return std::make_unique<SyncAlgorithm>();
}

std::unique_ptr<ICommandDispatcher> create_command_dispatcher()
{
    return std::make_unique<CommandDispatcher>();
}

std::unique_ptr<IEngineObserver> create_engine_observer()
{
    return std::make_unique<EngineObserver>();
}

} // namespace Prism::Business
