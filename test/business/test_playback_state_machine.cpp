#include <gtest/gtest.h>

#include "SyncFactory.h"

namespace {

using Prism::Business::SyncState;

TEST(PlaybackStateMachine, InitialStateIsUninit) {
    auto sm = Prism::Business::create_playback_state_machine();
    ASSERT_NE(sm, nullptr);
    EXPECT_EQ(sm->get_state(), SyncState::UNINIT);
}

TEST(PlaybackStateMachine, CalibratingToSynchronized) {
    auto sm = Prism::Business::create_playback_state_machine();
    ASSERT_NE(sm, nullptr);

    EXPECT_TRUE(sm->transition(SyncState::CALIBATING));
    EXPECT_TRUE(sm->transition(SyncState::SYNCHRONIZED));
    EXPECT_EQ(sm->get_state(), SyncState::SYNCHRONIZED);
}

TEST(PlaybackStateMachine, SynchronizedToAhead) {
    auto sm = Prism::Business::create_playback_state_machine();
    ASSERT_NE(sm, nullptr);

    sm->transition(SyncState::CALIBATING);
    sm->transition(SyncState::SYNCHRONIZED);
    EXPECT_TRUE(sm->transition(SyncState::AHEAD));
    EXPECT_EQ(sm->get_state(), SyncState::AHEAD);
}

TEST(PlaybackStateMachine, SynchronizedToBehind) {
    auto sm = Prism::Business::create_playback_state_machine();
    ASSERT_NE(sm, nullptr);

    sm->transition(SyncState::CALIBATING);
    sm->transition(SyncState::SYNCHRONIZED);
    EXPECT_TRUE(sm->transition(SyncState::BEHIND));
    EXPECT_EQ(sm->get_state(), SyncState::BEHIND);
}

} // namespace
