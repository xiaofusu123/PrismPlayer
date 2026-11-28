#include <gtest/gtest.h>

#include "AudioEngine.h"
#include "AudioEngineFactory.h"
#include "IEngineObserver.h"
#include "IEngineObserverSink.h"
#include "Player.h"
#include "VideoEngine.h"
#include "VideoEngineFactory.h"

#include <chrono>
#include <memory>

namespace {

constexpr int64_t K_SEEK_TARGET_MS = 5000;
constexpr int64_t K_SEEK_TOLERANCE_MS = 5;

/** 假音频引擎：实现观察者槽，由用例驱动 advance_to 推进 PTS。 */
class FakeAudioEngine : public Prism::Engine::AudioEngine,
                        public Prism::Business::IEngineObserverSink {
  public:
    bool init() override { return true; }
    bool play() override { return true; }
    bool pause() override { return true; }
    bool close() override { return true; }
    bool set_play_speed(float) override { return true; }
    bool seek(std::chrono::milliseconds pts) override {
        current_ms_ = pts.count();
        info_.current_pts.store(pts);
        return true;
    }
    Prism::Engine::AudioSyncInfo get_sync_info() override { return Prism::Engine::AudioSyncInfo{}; }

    void set_frame_observer(Prism::Business::IEngineObserver* observer) override {
        observer_ = observer;
    }

    void advance_to(int64_t pts_ms) {
        current_ms_ = pts_ms;
        info_.current_pts.store(std::chrono::milliseconds{pts_ms});
        if (observer_) {
            observer_->on_audio_frame(info_);
        }
    }

  private:
    Prism::Engine::AudioSyncInfo info_;
    Prism::Business::IEngineObserver* observer_{nullptr};
    int64_t current_ms_{0};
};

/** 假视频引擎：实现观察者槽，推视频 PTS，驱动同步状态机。 */
class FakeVideoEngine : public Prism::Engine::VideoEngine,
                        public Prism::Business::IEngineObserverSink {
  public:
    bool init() override { return true; }
    bool play() override { return true; }
    bool pause() override { return true; }
    bool close() override { return true; }
    bool set_play_speed(float) override { return true; }
    bool seek(std::chrono::milliseconds pts) override {
        current_ms_ = pts.count();
        return true;
    }
    Prism::Engine::VideoSyncInfo get_sync_info() override { return Prism::Engine::VideoSyncInfo{}; }
    Prism::Engine::RenderMetadata get_render_result() override { return {}; }

    void set_frame_observer(Prism::Business::IEngineObserver* observer) override {
        observer_ = observer;
    }

    void advance_to(int64_t pts_ms) {
        current_ms_ = pts_ms;
        info_.current_pts.store(std::chrono::milliseconds{pts_ms});
        if (observer_) {
            observer_->on_video_frame(info_);
        }
    }

  private:
    Prism::Engine::VideoSyncInfo info_;
    Prism::Business::IEngineObserver* observer_{nullptr};
    int64_t current_ms_{0};
};

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

/**
 * 位置语义用例：断言仅经 32 个 C ABI 完成，不 include 任何 service 内部头。
 * 引擎工厂按 C ABI 注入，假引擎实现观察者槽，由装配层注册同步观察者。
 */
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

    void push_frame(int64_t pts_ms) {
        audio_factory_.engine()->advance_to(pts_ms);
        video_factory_.engine()->advance_to(pts_ms);
    }

    PrismPlayerHandle player_{nullptr};
    FakeAudioFactory audio_factory_;
    FakeVideoFactory video_factory_;
};

TEST_F(PlayerPositionTest, PositionAdvancesMonotonicallyWhilePlaying) {
    int64_t prev = -1;
    for (int64_t pts : {1000LL, 1020LL, 1040LL, 1060LL}) {
        push_frame(pts);
        int64_t pos = prism_player_get_position(player_);
        EXPECT_EQ(pos, pts);
        EXPECT_GT(pos, prev);
        prev = pos;
    }
}

TEST_F(PlayerPositionTest, PauseFreezesAndResumeContinues) {
    push_frame(1000);
    EXPECT_EQ(prism_player_get_position(player_), 1000);

    ASSERT_EQ(prism_player_pause(player_), PRISM_OK);
    EXPECT_EQ(prism_player_get_state(player_), PRISM_STATE_PAUSED);
    push_frame(2000);
    EXPECT_EQ(prism_player_get_position(player_), 1000);

    ASSERT_EQ(prism_player_play(player_), PRISM_OK);
    EXPECT_EQ(prism_player_get_state(player_), PRISM_STATE_PLAYING);
    push_frame(3000);
    EXPECT_EQ(prism_player_get_position(player_), 3000);
}

TEST_F(PlayerPositionTest, SeekLandsWithinTolerance) {
    push_frame(1000);
    EXPECT_EQ(prism_player_get_position(player_), 1000);

    ASSERT_EQ(prism_player_seek(player_, K_SEEK_TARGET_MS, PRISM_SEEK_ABSOLUTE), PRISM_OK);
    push_frame(K_SEEK_TARGET_MS);

    int64_t pos = prism_player_get_position(player_);
    int64_t error = pos > K_SEEK_TARGET_MS ? pos - K_SEEK_TARGET_MS : K_SEEK_TARGET_MS - pos;
    EXPECT_LE(error, K_SEEK_TOLERANCE_MS);
}

} // namespace
