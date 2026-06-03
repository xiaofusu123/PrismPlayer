#include <gtest/gtest.h>

#include "SyncTypes.h"
#include "IPlaybackStateMachine.h"
#include "PlaybackStateMachine.h"

using namespace Prism::Business;

class PlaybackStateMachineTest : public ::testing::Test {
protected:
    void SetUp() override { sm_ = std::make_unique<PlaybackStateMachine>(); }
    void TearDown() override { sm_.reset(); }
    std::unique_ptr<IPlaybackStateMachine> sm_;
};

/* ========== Initial state ========== */

TEST_F(PlaybackStateMachineTest, initial_state_is_uninit)
{
    EXPECT_EQ(sm_->get_state(), SyncState::UNINIT);
}

/* ========== Valid transitions ========== */

TEST_F(PlaybackStateMachineTest, uninit_to_calibrating)
{
    EXPECT_TRUE(sm_->transition(SyncState::CALIBATING));
    EXPECT_EQ(sm_->get_state(), SyncState::CALIBATING);
}

TEST_F(PlaybackStateMachineTest, calibrating_to_synchronized)
{
    sm_->transition(SyncState::CALIBATING);
    EXPECT_TRUE(sm_->transition(SyncState::SYNCHRONIZED));
    EXPECT_EQ(sm_->get_state(), SyncState::SYNCHRONIZED);
}

TEST_F(PlaybackStateMachineTest, synchronized_to_ahead)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    EXPECT_TRUE(sm_->transition(SyncState::AHEAD));
    EXPECT_EQ(sm_->get_state(), SyncState::AHEAD);
}

TEST_F(PlaybackStateMachineTest, synchronized_to_behind)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    EXPECT_TRUE(sm_->transition(SyncState::BEHIND));
    EXPECT_EQ(sm_->get_state(), SyncState::BEHIND);
}

TEST_F(PlaybackStateMachineTest, ahead_to_synchronized)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    sm_->transition(SyncState::AHEAD);
    EXPECT_TRUE(sm_->transition(SyncState::SYNCHRONIZED));
    EXPECT_EQ(sm_->get_state(), SyncState::SYNCHRONIZED);
}

TEST_F(PlaybackStateMachineTest, behind_to_synchronized)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    sm_->transition(SyncState::BEHIND);
    EXPECT_TRUE(sm_->transition(SyncState::SYNCHRONIZED));
    EXPECT_EQ(sm_->get_state(), SyncState::SYNCHRONIZED);
}

TEST_F(PlaybackStateMachineTest, ahead_to_behind)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    sm_->transition(SyncState::AHEAD);
    EXPECT_TRUE(sm_->transition(SyncState::BEHIND));
    EXPECT_EQ(sm_->get_state(), SyncState::BEHIND);
}

TEST_F(PlaybackStateMachineTest, behind_to_ahead)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    sm_->transition(SyncState::BEHIND);
    EXPECT_TRUE(sm_->transition(SyncState::AHEAD));
    EXPECT_EQ(sm_->get_state(), SyncState::AHEAD);
}

TEST_F(PlaybackStateMachineTest, any_state_to_uninit)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    sm_->transition(SyncState::AHEAD);
    EXPECT_TRUE(sm_->transition(SyncState::UNINIT));
    EXPECT_EQ(sm_->get_state(), SyncState::UNINIT);
}

TEST_F(PlaybackStateMachineTest, uninit_to_disable)
{
    EXPECT_TRUE(sm_->transition(SyncState::DISABLE));
    EXPECT_EQ(sm_->get_state(), SyncState::DISABLE);
}

TEST_F(PlaybackStateMachineTest, any_state_to_sync_error)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    EXPECT_TRUE(sm_->transition(SyncState::SYNC_ERROR));
    EXPECT_EQ(sm_->get_state(), SyncState::SYNC_ERROR);
}

TEST_F(PlaybackStateMachineTest, sync_error_to_uninit)
{
    sm_->transition(SyncState::SYNC_ERROR);
    EXPECT_TRUE(sm_->transition(SyncState::UNINIT));
    EXPECT_EQ(sm_->get_state(), SyncState::UNINIT);
}

TEST_F(PlaybackStateMachineTest, synchronized_back_to_calibrating)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    EXPECT_TRUE(sm_->transition(SyncState::CALIBATING));
    EXPECT_EQ(sm_->get_state(), SyncState::CALIBATING);
}

/* ========== Same-state no-op ========== */

TEST_F(PlaybackStateMachineTest, same_state_transition_is_noop)
{
    EXPECT_TRUE(sm_->transition(SyncState::UNINIT));
    EXPECT_EQ(sm_->get_state(), SyncState::UNINIT);

    sm_->transition(SyncState::CALIBATING);
    EXPECT_TRUE(sm_->transition(SyncState::CALIBATING));
    EXPECT_EQ(sm_->get_state(), SyncState::CALIBATING);
}

/* ========== Invalid transitions ========== */

TEST_F(PlaybackStateMachineTest, uninit_to_synchronized_is_invalid)
{
    EXPECT_FALSE(sm_->transition(SyncState::SYNCHRONIZED));
    EXPECT_EQ(sm_->get_state(), SyncState::UNINIT);
}

TEST_F(PlaybackStateMachineTest, calibrating_to_ahead_is_invalid)
{
    sm_->transition(SyncState::CALIBATING);
    EXPECT_FALSE(sm_->transition(SyncState::AHEAD));
    EXPECT_EQ(sm_->get_state(), SyncState::CALIBATING);
}

TEST_F(PlaybackStateMachineTest, calibrating_to_behind_is_invalid)
{
    sm_->transition(SyncState::CALIBATING);
    EXPECT_FALSE(sm_->transition(SyncState::BEHIND));
    EXPECT_EQ(sm_->get_state(), SyncState::CALIBATING);
}

TEST_F(PlaybackStateMachineTest, disable_to_synchronized_is_invalid)
{
    sm_->transition(SyncState::DISABLE);
    EXPECT_FALSE(sm_->transition(SyncState::SYNCHRONIZED));
    EXPECT_EQ(sm_->get_state(), SyncState::DISABLE);
}

TEST_F(PlaybackStateMachineTest, synchronized_to_disable_is_invalid)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    EXPECT_FALSE(sm_->transition(SyncState::DISABLE));
    EXPECT_EQ(sm_->get_state(), SyncState::SYNCHRONIZED);
}

/* ========== can_play ========== */

TEST_F(PlaybackStateMachineTest, can_play_when_synchronized)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    EXPECT_TRUE(sm_->can_play());
}

TEST_F(PlaybackStateMachineTest, can_play_when_ahead)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    sm_->transition(SyncState::AHEAD);
    EXPECT_TRUE(sm_->can_play());
}

TEST_F(PlaybackStateMachineTest, can_play_when_behind)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    sm_->transition(SyncState::BEHIND);
    EXPECT_TRUE(sm_->can_play());
}

TEST_F(PlaybackStateMachineTest, can_play_when_disabled)
{
    sm_->transition(SyncState::DISABLE);
    EXPECT_TRUE(sm_->can_play());
}

TEST_F(PlaybackStateMachineTest, cannot_play_when_uninit)
{
    EXPECT_FALSE(sm_->can_play());
}

TEST_F(PlaybackStateMachineTest, cannot_play_when_calibrating)
{
    sm_->transition(SyncState::CALIBATING);
    EXPECT_FALSE(sm_->can_play());
}

TEST_F(PlaybackStateMachineTest, cannot_play_when_sync_error)
{
    sm_->transition(SyncState::SYNC_ERROR);
    EXPECT_FALSE(sm_->can_play());
}

/* ========== can_seek ========== */

TEST_F(PlaybackStateMachineTest, can_seek_when_synchronized)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    EXPECT_TRUE(sm_->can_seek());
}

TEST_F(PlaybackStateMachineTest, can_seek_when_calibrating)
{
    sm_->transition(SyncState::CALIBATING);
    EXPECT_TRUE(sm_->can_seek());
}

TEST_F(PlaybackStateMachineTest, can_seek_when_ahead)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    sm_->transition(SyncState::AHEAD);
    EXPECT_TRUE(sm_->can_seek());
}

TEST_F(PlaybackStateMachineTest, can_seek_when_disabled)
{
    sm_->transition(SyncState::DISABLE);
    EXPECT_TRUE(sm_->can_seek());
}

TEST_F(PlaybackStateMachineTest, cannot_seek_when_uninit)
{
    EXPECT_FALSE(sm_->can_seek());
}

TEST_F(PlaybackStateMachineTest, cannot_seek_when_sync_error)
{
    sm_->transition(SyncState::SYNC_ERROR);
    EXPECT_FALSE(sm_->can_seek());
}

/* ========== reset ========== */

TEST_F(PlaybackStateMachineTest, reset_returns_to_uninit)
{
    sm_->transition(SyncState::CALIBATING);
    sm_->transition(SyncState::SYNCHRONIZED);
    sm_->transition(SyncState::AHEAD);
    sm_->reset();
    EXPECT_EQ(sm_->get_state(), SyncState::UNINIT);
}

TEST_F(PlaybackStateMachineTest, reset_from_error_returns_to_uninit)
{
    sm_->transition(SyncState::SYNC_ERROR);
    sm_->reset();
    EXPECT_EQ(sm_->get_state(), SyncState::UNINIT);
}
