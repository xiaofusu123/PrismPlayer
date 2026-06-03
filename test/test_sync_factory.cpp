#include <gtest/gtest.h>

#include "SyncTypes.h"
#include "IPlaybackStateMachine.h"
#include "ISyncAlgorithm.h"
#include "ICommandDispatcher.h"
#include "IEngineObserver.h"
#include "SyncFactory.h"

using namespace Prism::Business;

/* ========== Factory creation ========== */

TEST(SyncFactoryTest, create_playback_state_machine_returns_valid_object)
{
    auto sm = create_playback_state_machine();
    ASSERT_NE(sm, nullptr);
    EXPECT_EQ(sm->get_state(), SyncState::UNINIT);
}

TEST(SyncFactoryTest, create_sync_algorithm_returns_valid_object)
{
    auto algo = create_sync_algorithm();
    ASSERT_NE(algo, nullptr);

    auto info = algo->get_drift_info();
    EXPECT_EQ(info.drift_ms, 0);

    SyncAction action = algo->calibrate(100, 120);
    EXPECT_EQ(action, SyncAction::RENDER);
}

TEST(SyncFactoryTest, create_command_dispatcher_returns_valid_object)
{
    auto dispatcher = create_command_dispatcher();
    ASSERT_NE(dispatcher, nullptr);

    // With null engines, dispatch should not crash
    bool result = dispatcher->dispatch_play();
    EXPECT_TRUE(result);
}

TEST(SyncFactoryTest, create_engine_observer_returns_valid_object)
{
    auto observer = create_engine_observer();
    ASSERT_NE(observer, nullptr);
}

/* ========== Cross-component wiring ========== */

TEST(SyncFactoryTest, observer_can_be_wired_with_state_machine_and_algorithm)
{
    auto sm       = create_playback_state_machine();
    auto algo     = create_sync_algorithm();
    auto observer = create_engine_observer();

    ASSERT_NE(sm, nullptr);
    ASSERT_NE(algo, nullptr);
    ASSERT_NE(observer, nullptr);

    observer->set_sync_algorithm(algo.get());
    observer->set_state_machine(sm.get());

    // Verify no crash on wiring
    SUCCEED();
}

TEST(SyncFactoryTest, dispatcher_can_be_wired_with_engines)
{
    auto dispatcher = create_command_dispatcher();
    ASSERT_NE(dispatcher, nullptr);

    // Setting nullptr engines should not crash (it's a no-op)
    dispatcher->set_audio_engine(nullptr);
    dispatcher->set_video_engine(nullptr);

    SUCCEED();
}
