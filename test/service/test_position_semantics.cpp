#include <gtest/gtest.h>

#include "AudioEngine.h"
#include "AudioEngineFactory.h"
#include "IEngineObserver.h"
#include "Player.h"
#include "VideoEngine.h"
#include "VideoEngineFactory.h"

#include <memory>

namespace {

constexpr int64_t k_seek_target_ms = 5000;
constexpr int64_t k_seek_tolerance_ms = 5;

/**
 * 假音频引擎：由测试驱动 advance_to()，把 AudioSyncInfo.current_pts
 * 经 IEngineObserver::on_audio_frame 送进同步侧。
 */
class FakeAudioEngine : public Prism::Engine::AudioEngine {
public:
    bool init() override { return true; }
    bool play() override { return true; }
    bool pause() override { return true; }
    bool close() override { return true; }
    bool set_play_speed() override { return true; }
    bool seek(uint64_t pts, int seek_mode) override {
        uint64_t cur = sync_info_.current_pts.load();
        sync_info_.current_pts.store(seek_mode == 0 ? pts : cur + pts);
        return true;
    }
    Prism::Engine::AudioSyncInfo get_sync_info() override {
        return Prism::Engine::AudioSyncInfo{};
    }
    void set_observer(Prism::Engine::IEngineObserver* observer) override {
        observer_ = observer;
    }

    void advance_to(uint64_t pts) {
        sync_info_.current_pts.store(pts);
        if (observer_) {
            observer_->on_audio_frame(sync_info_);
        }
    }

private:
    Prism::Engine::AudioSyncInfo sync_info_;
    Prism::Engine::IEngineObserver* observer_{nullptr};
};

/**
 * 假视频引擎：推进 VideoSyncInfo.current_pts 并回调 on_video_frame。
 */
class FakeVideoEngine : public Prism::Engine::VideoEngine {
public:
    bool init() override { return true; }
    bool play() override { return true; }
    bool pause() override { return true; }
    bool close() override { return true; }
    bool set_play_speed(float speed) override { return true; }
    bool seek(uint64_t pts, int seek_mode) override {
        uint64_t cur = sync_info_.current_pts.load();
        sync_info_.current_pts.store(seek_mode == 0 ? pts : cur + pts);
        return true;
    }
    Prism::Engine::VideoSyncInfo get_sync_info() override {
        return Prism::Engine::VideoSyncInfo{};
    }
    Prism::Engine::RenderMetadata get_render_result() override {
        return Prism::Engine::RenderMetadata{};
    }
    void set_observer(Prism::Engine::IEngineObserver* observer) override {
        observer_ = observer;
    }

    void advance_to(uint64_t pts) {
        sync_info_.current_pts.store(pts);
        if (observer_) {
            observer_->on_video_frame(sync_info_);
        }
    }

private:
    Prism::Engine::VideoSyncInfo sync_info_;
    Prism::Engine::IEngineObserver* observer_{nullptr};
};

/**
 * 假音频工厂：保留创建出的引擎裸指针，供测试驱动帧推进。
 */
class FakeAudioFactory : public Prism::Engine::AudioEngineFactory {
public:
    std::unique_ptr<Prism::Engine::AudioEngine> create_audio_engine() override {
        auto engine = std::make_unique<FakeAudioEngine>();
        engine_ = engine.get();
        return engine;
    }
    FakeAudioEngine* engine() { return engine_; }

private:
    FakeAudioEngine* engine_{nullptr};
};

/**
 * 假视频工厂：保留创建出的引擎裸指针，供测试驱动帧推进。
 */
class FakeVideoFactory : public Prism::Engine::VideoEngineFactory {
public:
    std::unique_ptr<Prism::Engine::VideoEngine> create_video_engine() override {
        auto engine = std::make_unique<FakeVideoEngine>();
        engine_ = engine.get();
        return engine;
    }
    FakeVideoEngine* engine() { return engine_; }

private:
    FakeVideoEngine* engine_{nullptr};
};

class PlayerPositionTest : public ::testing::Test {
protected:
    void SetUp() override {
        player_ = prism_player_create(nullptr, nullptr, nullptr);
        ASSERT_NE(player_, nullptr);
        ASSERT_EQ(prism_player_set_audio_factory(player_, &audio_factory_), PRISM_OK);
        ASSERT_EQ(prism_player_set_video_factory(player_, &video_factory_), PRISM_OK);
        ASSERT_EQ(prism_player_open(player_, "fake://media"), PRISM_OK);
        ASSERT_EQ(prism_player_play(player_), PRISM_OK);
    }

    void TearDown() override { prism_player_destroy(player_); }

    PrismPlayerHandle player_{nullptr};
    FakeAudioFactory audio_factory_;
    FakeVideoFactory video_factory_;
};

TEST_F(PlayerPositionTest, MonotonicAdvanceWhilePlaying) {
    int64_t prev = -1;
    for (uint64_t pts : {1000ULL, 1020ULL, 1040ULL, 1060ULL}) {
        audio_factory_.engine()->advance_to(pts);
        video_factory_.engine()->advance_to(pts);

        int64_t pos = prism_player_get_position(player_);
        EXPECT_EQ(pos, static_cast<int64_t>(pts));
        EXPECT_GT(pos, prev);
        prev = pos;
    }
}

TEST_F(PlayerPositionTest, PausesAndResumes) {
    audio_factory_.engine()->advance_to(1000);
    video_factory_.engine()->advance_to(1000);
    EXPECT_EQ(prism_player_get_position(player_), 1000);

    ASSERT_EQ(prism_player_pause(player_), PRISM_OK);
    EXPECT_EQ(prism_player_get_state(player_), PRISM_STATE_PAUSED);
    EXPECT_EQ(prism_player_get_position(player_), 1000);

    ASSERT_EQ(prism_player_play(player_), PRISM_OK);
    EXPECT_EQ(prism_player_get_state(player_), PRISM_STATE_PLAYING);

    audio_factory_.engine()->advance_to(2000);
    video_factory_.engine()->advance_to(2000);
    EXPECT_EQ(prism_player_get_position(player_), 2000);
}

TEST_F(PlayerPositionTest, SeekLandsWithinTolerance) {
    audio_factory_.engine()->advance_to(1000);
    video_factory_.engine()->advance_to(1000);
    EXPECT_EQ(prism_player_get_position(player_), 1000);

    ASSERT_EQ(prism_player_seek(player_, k_seek_target_ms, PRISM_SEEK_ABSOLUTE), PRISM_OK);

    audio_factory_.engine()->advance_to(k_seek_target_ms);
    video_factory_.engine()->advance_to(k_seek_target_ms);

    int64_t pos = prism_player_get_position(player_);
    int64_t error = pos > k_seek_target_ms ? pos - k_seek_target_ms : k_seek_target_ms - pos;
    EXPECT_LE(error, k_seek_tolerance_ms);
}

} // namespace
