#include <gtest/gtest.h>

#include "SyncFactory.h"

namespace {

using Prism::Business::SyncAction;
using Prism::Business::SyncConfig;

TEST(SyncAlgorithm, DriftIsVideoMinusAudio) {
    auto algo = Prism::Business::create_sync_algorithm();
    ASSERT_NE(algo, nullptr);

    algo->calibrate(1000, 1000);
    EXPECT_EQ(algo->get_drift_info().drift_ms, 0);

    algo->calibrate(1000, 1080);
    EXPECT_EQ(algo->get_drift_info().drift_ms, 80);

    algo->calibrate(1000, 900);
    EXPECT_EQ(algo->get_drift_info().drift_ms, -100);
}

TEST(SyncAlgorithm, ActionOnBothSidesOfAheadThreshold) {
    auto algo = Prism::Business::create_sync_algorithm();
    SyncConfig config;
    config.ahead_threshold_ms = 30;
    config.behind_threshold_ms = 50;
    algo->configure(config);

    EXPECT_EQ(algo->calibrate(1000, 1030), SyncAction::RENDER);
    EXPECT_EQ(algo->calibrate(1000, 1031), SyncAction::WAIT);
}

TEST(SyncAlgorithm, ActionOnBothSidesOfBehindThreshold) {
    auto algo = Prism::Business::create_sync_algorithm();
    SyncConfig config;
    config.ahead_threshold_ms = 30;
    config.behind_threshold_ms = 50;
    algo->configure(config);

    EXPECT_EQ(algo->calibrate(1000, 950), SyncAction::RENDER);
    EXPECT_EQ(algo->calibrate(1000, 949), SyncAction::DROP);
}

TEST(SyncAlgorithm, UpdateAudioPtsPublishesPosition) {
    auto algo = Prism::Business::create_sync_algorithm();
    ASSERT_NE(algo, nullptr);

    algo->update_audio_pts(1500);
    auto drift = algo->get_drift_info();
    EXPECT_EQ(drift.audio_pts, 1500U);
    EXPECT_EQ(drift.drift_ms, 0);
    EXPECT_EQ(drift.video_pts, 0U);
}

} // namespace
