#include <gtest/gtest.h>

#include "AudioEngine.h"
#include "IEngineObserver.h"
#include "IPlaybackStateMachine.h"
#include "ISyncAlgorithm.h"
#include "ISyncObserver.h"
#include "SyncFactory.h"
#include "VideoEngine.h"

#include <memory>

namespace {

using Prism::Business::SyncAction;
using Prism::Business::SyncState;

/**
 * 假音频引擎：由测试驱动 advance_to()，把推进的 AudioSyncInfo.current_pts
 * 经 IEngineObserver::on_audio_frame 送进同步侧（对应 #55 的「引擎 → AV Sync 回调通道」）。
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
    Prism::Engine::AudioSyncInfo get_sync_info() override { return Prism::Engine::AudioSyncInfo{}; }
    void set_observer(Prism::Engine::IEngineObserver* observer) override { observer_ = observer; }

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
 * 假视频引擎：推进 VideoSyncInfo.current_pts 并回调 on_video_frame，
 * 同时按同步算法给出的 SyncAction 记录 presented / waited / dropped 计数。
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
    Prism::Engine::VideoSyncInfo get_sync_info() override { return Prism::Engine::VideoSyncInfo{}; }
    Prism::Engine::RenderMetadata get_render_result() override {
        return Prism::Engine::RenderMetadata{};
    }
    void set_observer(Prism::Engine::IEngineObserver* observer) override { observer_ = observer; }

    void advance_to(uint64_t pts) {
        sync_info_.current_pts.store(pts);
        if (observer_) {
            observer_->on_video_frame(sync_info_);
        }
    }

    void record_decision(SyncAction action) {
        switch (action) {
        case SyncAction::RENDER:
            ++presented_;
            break;
        case SyncAction::WAIT:
            ++waited_;
            break;
        case SyncAction::DROP:
            ++dropped_;
            break;
        }
    }
    [[nodiscard]] int presented() const { return presented_; }
    [[nodiscard]] int waited() const { return waited_; }
    [[nodiscard]] int dropped() const { return dropped_; }

  private:
    Prism::Engine::VideoSyncInfo sync_info_;
    Prism::Engine::IEngineObserver* observer_{nullptr};
    int presented_{0};
    int waited_{0};
    int dropped_{0};
};

/**
 * 装配同步侧组件与假引擎，模拟 Service 层装配时的依赖关系。
 */
struct SyncHarness {
    SyncHarness() {
        algo = Prism::Business::create_sync_algorithm();
        sm = Prism::Business::create_playback_state_machine();
        observer = Prism::Business::create_engine_observer();
        observer->set_sync_algorithm(algo.get());
        observer->set_state_machine(sm.get());
        audio.set_observer(observer.get());
        video.set_observer(observer.get());
    }

    std::unique_ptr<Prism::Business::ISyncAlgorithm> algo;
    std::unique_ptr<Prism::Business::IPlaybackStateMachine> sm;
    std::unique_ptr<Prism::Business::ISyncObserver> observer;
    FakeAudioEngine audio;
    FakeVideoEngine video;
};

/**
 * 推一帧（音频先到、视频后到），校验同步算法给出的 SyncAction 与状态机落点一致，
 * 并把该决策记录进假视频引擎的计数。
 */
void drive_frame(SyncHarness& h, uint64_t audio_pts, uint64_t video_pts, SyncAction expected_action,
                 SyncState expected_state) {
    h.audio.advance_to(audio_pts);
    h.video.advance_to(video_pts);

    SyncAction action = h.algo->calibrate(audio_pts, video_pts);
    EXPECT_EQ(action, expected_action);
    EXPECT_EQ(h.sm->get_state(), expected_state);

    h.video.record_decision(action);
}

struct DecisionLoopSnapshot {
    int64_t drift_ms;
    SyncState state;
    int presented;
    int waited;
    int dropped;
};

DecisionLoopSnapshot run_decision_loop() {
    SyncHarness h;

    // 首帧：音频帧触发 UNINIT → CALIBATING，视频帧落到首个同步点 SYNCHRONIZED。
    h.audio.advance_to(1000);
    EXPECT_EQ(h.sm->get_state(), SyncState::CALIBATING);
    h.video.advance_to(1000);
    EXPECT_EQ(h.sm->get_state(), SyncState::SYNCHRONIZED);

    // 固定序列 RENDER / WAIT / DROP / RENDER，覆盖三种 SyncAction 与状态迁移。
    drive_frame(h, 2000, 2000, SyncAction::RENDER, SyncState::SYNCHRONIZED);
    drive_frame(h, 3000, 3040, SyncAction::WAIT, SyncState::AHEAD);
    drive_frame(h, 4000, 3940, SyncAction::DROP, SyncState::BEHIND);
    drive_frame(h, 5000, 5000, SyncAction::RENDER, SyncState::SYNCHRONIZED);

    DecisionLoopSnapshot s;
    s.drift_ms = h.algo->get_drift_info().drift_ms;
    s.state = h.sm->get_state();
    s.presented = h.video.presented();
    s.waited = h.video.waited();
    s.dropped = h.video.dropped();
    return s;
}

TEST(EngineObserver, AudioFrameTransitionsUninitToCalibrating) {
    SyncHarness h;
    EXPECT_EQ(h.sm->get_state(), SyncState::UNINIT);

    h.audio.advance_to(0);
    EXPECT_EQ(h.sm->get_state(), SyncState::CALIBATING);
}

TEST(EngineObserver, VideoFrameComputesRealDrift) {
    SyncHarness h;
    h.audio.advance_to(1000);
    EXPECT_EQ(h.sm->get_state(), SyncState::CALIBATING);

    h.video.advance_to(1030);
    EXPECT_EQ(h.sm->get_state(), SyncState::SYNCHRONIZED);

    auto drift = h.algo->get_drift_info();
    EXPECT_EQ(drift.drift_ms, 30);
    EXPECT_EQ(drift.audio_pts, 1000ULL);
    EXPECT_EQ(drift.video_pts, 1030ULL);
}

TEST(SyncAlgorithm, AheadThresholdBoundaryRenders) {
    auto algo = Prism::Business::create_sync_algorithm();
    // drift == +30 == ahead_threshold_ms，未越过阈值 → RENDER。
    EXPECT_EQ(algo->calibrate(1000, 1030), SyncAction::RENDER);
}

TEST(SyncAlgorithm, AheadThresholdBeyondWaits) {
    auto algo = Prism::Business::create_sync_algorithm();
    // drift == +31，刚越过 ahead_threshold_ms → WAIT。
    EXPECT_EQ(algo->calibrate(1000, 1031), SyncAction::WAIT);
}

TEST(SyncAlgorithm, BehindThresholdBoundaryRenders) {
    auto algo = Prism::Business::create_sync_algorithm();
    // drift == -50 == -behind_threshold_ms，未越过阈值 → RENDER。
    EXPECT_EQ(algo->calibrate(1000, 950), SyncAction::RENDER);
}

TEST(SyncAlgorithm, BehindThresholdBeyondDrops) {
    auto algo = Prism::Business::create_sync_algorithm();
    // drift == -51，刚越过 -behind_threshold_ms → DROP。
    EXPECT_EQ(algo->calibrate(1000, 949), SyncAction::DROP);
}

TEST(EngineObserver, DecisionLoopCountsMatchSyncAction) {
    DecisionLoopSnapshot s = run_decision_loop();

    EXPECT_EQ(s.presented, 2);
    EXPECT_EQ(s.waited, 1);
    EXPECT_EQ(s.dropped, 1);
}

TEST(EngineObserver, DecisionLoopIsStableAcrossThreeRuns) {
    DecisionLoopSnapshot a = run_decision_loop();
    DecisionLoopSnapshot b = run_decision_loop();
    DecisionLoopSnapshot c = run_decision_loop();

    EXPECT_EQ(a.drift_ms, b.drift_ms);
    EXPECT_EQ(a.state, b.state);
    EXPECT_EQ(a.presented, b.presented);
    EXPECT_EQ(a.waited, b.waited);
    EXPECT_EQ(a.dropped, b.dropped);

    EXPECT_EQ(b.drift_ms, c.drift_ms);
    EXPECT_EQ(b.state, c.state);
    EXPECT_EQ(b.presented, c.presented);
    EXPECT_EQ(b.waited, c.waited);
    EXPECT_EQ(b.dropped, c.dropped);
}

} // namespace
