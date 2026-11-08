#include <gtest/gtest.h>

#include "AudioEngineFactory.h"
#include "EngineObserver.h"
#include "Player.h"
#include "VideoEngineFactory.h"

#include <atomic>
#include <memory>

namespace {

/**
 * @class FakeAudioEngine
 * @brief 由虚拟时钟驱动的假音频引擎
 *
 * 测试通过 push_audio_frame 手动推进 PTS 并触发真实观察者回调，
 * 不依赖真实墙钟。pause 后 push_audio_frame 不再触发回调，模拟暂停停住。
 */
class FakeAudioEngine : public Prism::Engine::AudioEngine {
  public:
    bool init() override { return true; }
    bool play() override {
        paused_.store(false);
        return true;
    }
    bool pause() override {
        paused_.store(true);
        return true;
    }
    bool close() override { return true; }
    bool set_play_speed() override { return true; }
    bool seek(uint64_t pts, int) override {
        current_pts_.store(pts);
        return true;
    }
    Prism::Engine::AudioSyncInfo get_sync_info() override {
        return {};
    }
    void set_observer(Prism::Engine::EngineObserver* observer) override { observer_ = observer; }

    void push_audio_frame(uint64_t pts) {
        if (paused_.load()) {
            return;
        }
        current_pts_.store(pts);
        if (observer_) {
            Prism::Engine::AudioSyncInfo info;
            info.current_pts.store(pts);
            observer_->on_audio_frame(info);
        }
    }

  private:
    Prism::Engine::EngineObserver* observer_{nullptr};
    std::atomic<uint64_t> current_pts_{0};
    std::atomic<bool> paused_{false};
};

class FakeAudioFactory : public Prism::Engine::AudioEngineFactory {
  public:
    std::unique_ptr<Prism::Engine::AudioEngine> create_audio_engine() override {
        auto engine = std::make_unique<FakeAudioEngine>();
        last_engine_ = engine.get();
        return engine;
    }
    [[nodiscard]] FakeAudioEngine* last_engine() const { return last_engine_; }

  private:
    FakeAudioEngine* last_engine_{nullptr};
};

class FakeVideoEngine : public Prism::Engine::VideoEngine {
  public:
    bool init() override { return true; }
    bool play() override { return true; }
    bool pause() override { return true; }
    bool close() override { return true; }
    bool set_play_speed(float) override { return true; }
    bool seek(uint64_t, int) override { return true; }
    Prism::Engine::VideoSyncInfo get_sync_info() override { return {}; }
    Prism::Engine::RenderMetadata get_render_result() override { return {}; }
    void set_observer(Prism::Engine::EngineObserver* observer) override { observer_ = observer; }

  private:
    Prism::Engine::EngineObserver* observer_{nullptr};
};

class FakeVideoFactory : public Prism::Engine::VideoEngineFactory {
  public:
    std::unique_ptr<Prism::Engine::VideoEngine> create_video_engine() override {
        return std::make_unique<FakeVideoEngine>();
    }
};

// 三个用例均由虚拟时钟驱动（手动 push_audio_frame，无真实墙钟），
// 连跑多次结果一致（可配合 --gtest_repeat=3 验证稳定性）。
class PlayerPositionTest : public ::testing::Test {
  protected:
    void SetUp() override {
        player_ = prism_player_create(nullptr, nullptr, nullptr);
        ASSERT_NE(player_, nullptr);
        ASSERT_EQ(prism_player_set_audio_factory(player_, &audio_factory_), PRISM_OK);
        ASSERT_EQ(prism_player_set_video_factory(player_, &video_factory_), PRISM_OK);
        ASSERT_EQ(prism_player_open(player_, "test://media"), PRISM_OK);
        ASSERT_EQ(prism_player_play(player_), PRISM_OK);
        audio_ = audio_factory_.last_engine();
        ASSERT_NE(audio_, nullptr);
    }

    void TearDown() override {
        if (player_) {
            prism_player_destroy(player_);
        }
    }

    PrismPlayerHandle player_{nullptr};
    FakeAudioFactory audio_factory_;
    FakeVideoFactory video_factory_;
    FakeAudioEngine* audio_{nullptr};
};

TEST_F(PlayerPositionTest, AdvancesWithAudioPts) {
    audio_->push_audio_frame(1000);
    EXPECT_EQ(prism_player_get_position(player_), 1000);

    audio_->push_audio_frame(2000);
    EXPECT_EQ(prism_player_get_position(player_), 2000);
}

TEST_F(PlayerPositionTest, StopsOnPauseAndResumes) {
    audio_->push_audio_frame(1000);
    ASSERT_EQ(prism_player_pause(player_), PRISM_OK);

    audio_->push_audio_frame(2000);
    EXPECT_EQ(prism_player_get_position(player_), 1000);

    ASSERT_EQ(prism_player_play(player_), PRISM_OK);
    audio_->push_audio_frame(3000);
    EXPECT_EQ(prism_player_get_position(player_), 3000);
}

TEST_F(PlayerPositionTest, SeekLandsWithinTolerance) {
    audio_->push_audio_frame(1000);
    ASSERT_EQ(prism_player_seek(player_, 5000, PRISM_SEEK_ABSOLUTE), PRISM_OK);

    audio_->push_audio_frame(5000);
    EXPECT_NEAR(prism_player_get_position(player_), 5000, 1);
}

} // namespace
