#include <gtest/gtest.h>

#include "EngineObserver.h"
#include "SyncFactory.h"

namespace {

using Prism::Business::SyncConfig;
using Prism::Business::SyncState;

/**
 * @class FakeVideoEngine
 * @brief 记录 RENDER/WAIT/DROP 对应计数的假视频引擎
 *
 * 帧到达时触发真实 EngineObserver 的回调，回调结束后读取真实状态机的状态，
 * 把 SYNCHRONIZED/AHEAD/BEHIND 翻译为 presented/waited/dropped 计数，
 * 从而断言 SyncAction（RENDER/WAIT/DROP）真的作用到了帧上。
 */
class FakeVideoEngine : public Prism::Engine::VideoEngine {
  public:
    void set_state_machine(Prism::Business::IPlaybackStateMachine* sm) { state_machine_ = sm; }

    [[nodiscard]] int presented() const { return presented_; }
    [[nodiscard]] int waited() const { return waited_; }
    [[nodiscard]] int dropped() const { return dropped_; }

    void deliver_frame(uint64_t video_pts) {
        Prism::Engine::VideoSyncInfo info;
        info.current_pts.store(video_pts);
        if (observer_) {
            observer_->on_video_frame(info);
        }
        if (state_machine_) {
            switch (state_machine_->get_state()) {
            case SyncState::SYNCHRONIZED:
                presented_++;
                break;
            case SyncState::AHEAD:
                waited_++;
                break;
            case SyncState::BEHIND:
                dropped_++;
                break;
            default:
                break;
            }
        }
    }

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
    Prism::Business::IPlaybackStateMachine* state_machine_{nullptr};
    int presented_{0};
    int waited_{0};
    int dropped_{0};
};

TEST(EngineObserver, DriftIsRealAndActionsMapToCounts) {
    auto algo = Prism::Business::create_sync_algorithm();
    auto sm = Prism::Business::create_playback_state_machine();
    auto observer = Prism::Business::create_engine_observer();
    ASSERT_NE(algo, nullptr);
    ASSERT_NE(sm, nullptr);
    ASSERT_NE(observer, nullptr);

    SyncConfig config;
    config.ahead_threshold_ms = 30;
    config.behind_threshold_ms = 50;
    algo->configure(config);

    observer->set_sync_algorithm(algo.get());
    observer->set_state_machine(sm.get());

    FakeVideoEngine video;
    video.set_observer(observer.get());
    video.set_state_machine(sm.get());

    Prism::Engine::AudioSyncInfo audio_info;
    audio_info.current_pts.store(1000);
    observer->on_audio_frame(audio_info);
    EXPECT_EQ(sm->get_state(), SyncState::CALIBATING);

    video.deliver_frame(1000);
    EXPECT_EQ(sm->get_state(), SyncState::SYNCHRONIZED);
    EXPECT_EQ(algo->get_drift_info().drift_ms, 0);
    EXPECT_EQ(video.presented(), 1);

    video.deliver_frame(1100);
    EXPECT_EQ(sm->get_state(), SyncState::AHEAD);
    EXPECT_EQ(algo->get_drift_info().drift_ms, 100);
    EXPECT_EQ(video.waited(), 1);

    video.deliver_frame(900);
    EXPECT_EQ(sm->get_state(), SyncState::BEHIND);
    EXPECT_EQ(algo->get_drift_info().drift_ms, -100);
    EXPECT_EQ(video.dropped(), 1);
}

} // namespace
