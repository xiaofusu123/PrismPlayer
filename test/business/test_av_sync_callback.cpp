#include <gtest/gtest.h>

#include "AudioEngine.h"
#include "IEngineObserver.h"
#include "IEngineObserverSink.h"
#include "IPlaybackStateMachine.h"
#include "ISyncAlgorithm.h"
#include "ISyncClock.h"
#include "SyncFactory.h"
#include "SyncTypes.h"
#include "VideoEngine.h"

#include <chrono>
#include <memory>

namespace {

using Prism::Business::ISyncAlgorithm;
using Prism::Business::SyncAction;
using Prism::Business::SyncState;

/** 虚拟时钟：用例手动推进，不依赖真实墙钟，保证连跑一致。 */
class VirtualClock : public Prism::Business::ISyncClock {
  public:
    void advance(uint64_t delta_ms) { now_ms_ += delta_ms; }
    void set(uint64_t now_ms) { now_ms_ = now_ms; }
    [[nodiscard]] uint64_t now_ms() const override { return now_ms_; }

  private:
    uint64_t now_ms_{0};
};

/** 假音频引擎：由用例驱动 advance_to，把 PTS 经观察者回调送进同步侧。 */
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
    [[nodiscard]] int64_t current() const { return current_ms_; }

  private:
    Prism::Engine::AudioSyncInfo info_;
    Prism::Business::IEngineObserver* observer_{nullptr};
    int64_t current_ms_{0};
};

/**
 * 假视频引擎：把 PTS 经回调送进同步侧，并在每帧后读取观察者决策，
 * 累计 presented / dropped / waited，供「决策闭环可观测」断言。
 */
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
            record(observer_->get_last_action());
        }
    }

    void record(SyncAction action) {
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
    Prism::Engine::VideoSyncInfo info_;
    Prism::Business::IEngineObserver* observer_{nullptr};
    int64_t current_ms_{0};
    int presented_{0};
    int waited_{0};
    int dropped_{0};
};

/** 装配同步侧四件套 + 假引擎，模拟 Service 的装配关系。 */
struct SyncHarness {
    SyncHarness() {
        algo = Prism::Business::create_sync_algorithm();
        sm = Prism::Business::create_playback_state_machine();
        observer = Prism::Business::create_engine_observer();
        observer->set_sync_algorithm(algo.get());
        observer->set_state_machine(sm.get());
        observer->set_clock(&clock);
        audio.set_frame_observer(observer.get());
        video.set_frame_observer(observer.get());
    }

    /** 退出初始校准：音频帧启动校准，推进虚拟时钟越过校准时长后再送一帧视频。 */
    void finish_calibration() {
        audio.advance_to(1000);
        EXPECT_EQ(sm->get_state(), SyncState::CALIBATING);
        video.advance_to(1000);
        EXPECT_EQ(sm->get_state(), SyncState::CALIBATING);
        clock.advance(500);
        video.advance_to(1000);
        ASSERT_EQ(sm->get_state(), SyncState::SYNCHRONIZED);
    }

    VirtualClock clock;
    std::unique_ptr<ISyncAlgorithm> algo;
    std::unique_ptr<Prism::Business::IPlaybackStateMachine> sm;
    std::unique_ptr<Prism::Business::IEngineObserver> observer;
    FakeAudioEngine audio;
    FakeVideoEngine video;
};

/** 决策闭环报告：drift 与 inter_client_skew_ms 是两个独立字段，不得混用。 */
struct DecisionLoopReport {
    int64_t drift_ms{0};
    int64_t inter_client_skew_ms{0};
    int presented{0};
    int waited{0};
    int dropped{0};
};

DecisionLoopReport run_decision_loop() {
    SyncHarness h;
    h.finish_calibration();

    // 固定序列：RENDER / WAIT / DROP / RENDER，覆盖三种决策与状态迁移。
    h.audio.advance_to(2000);
    h.video.advance_to(2000);
    h.audio.advance_to(3000);
    h.video.advance_to(3040);
    h.audio.advance_to(4000);
    h.video.advance_to(3940);
    h.audio.advance_to(5000);
    h.video.advance_to(5010); // +10，仍在阈值内，保持 RENDER 且 drift 非零

    DecisionLoopReport report;
    report.drift_ms = h.algo->get_drift_info().drift_ms;
    report.presented = h.video.presented();
    report.waited = h.video.waited();
    report.dropped = h.video.dropped();
    return report;
}

/* ==================== #129 回调通道 ==================== */

TEST(EngineObserver, VideoFrameDrivesRealDrift) {
    SyncHarness h;
    h.finish_calibration();
    h.audio.advance_to(7000);
    h.video.advance_to(7040);

    auto info = h.algo->get_drift_info();
    EXPECT_EQ(info.drift_ms, 40);
    EXPECT_EQ(info.audio_pts, 7000ULL);
    EXPECT_EQ(info.video_pts, 7040ULL);
}

/* ==================== #134 同步语义 ==================== */

TEST(SyncAlgorithm, ThresholdsAreRuntimeConfigurable) {
    auto algo = Prism::Business::create_sync_algorithm();
    Prism::Business::SyncConfig custom;
    custom.ahead_threshold_ms = 5;
    custom.behind_threshold_ms = 6;
    algo->configure(custom);

    EXPECT_EQ(algo->get_config().ahead_threshold_ms, 5U);
    EXPECT_EQ(algo->get_config().behind_threshold_ms, 6U);
    // 默认阈值 30 时 1031 才 WAIT；改为 5 后 +10 即 WAIT。
    EXPECT_EQ(algo->calibrate(1000, 1010), SyncAction::WAIT);
    EXPECT_EQ(algo->calibrate(1000, 993), SyncAction::DROP);
}

TEST(EngineObserver, CalibrationExitsByVirtualDurationAndDropsNothing) {
    SyncHarness h;
    h.audio.advance_to(1000);
    // 校准未到时长：即便 PTS 差很大，决策仍是 RENDER（不丢帧），状态保持 CALIBATING。
    h.video.advance_to(9000);
    EXPECT_EQ(h.observer->get_last_action(), SyncAction::RENDER);
    EXPECT_EQ(h.sm->get_state(), SyncState::CALIBATING);
    EXPECT_EQ(h.video.dropped(), 0);

    h.clock.advance(499);
    h.video.advance_to(9000);
    EXPECT_EQ(h.sm->get_state(), SyncState::CALIBATING);

    h.clock.advance(1);
    h.video.advance_to(9000);
    EXPECT_EQ(h.sm->get_state(), SyncState::SYNCHRONIZED);
}

TEST(EngineObserver, SeekResetClearsOldTimestamp) {
    SyncHarness h;
    h.finish_calibration();
    h.audio.advance_to(9000);
    h.video.advance_to(9040);
    ASSERT_EQ(h.algo->get_drift_info().audio_pts, 9000ULL);

    // seek：Service 同时重置状态机与观察者（算法与缓存 PTS 一并清空），
    // 不拿旧时间戳继续算。
    h.sm->reset();
    h.observer->reset();
    EXPECT_EQ(h.algo->get_drift_info().audio_pts, 0ULL);
    EXPECT_EQ(h.sm->get_state(), SyncState::UNINIT);

    h.audio.advance_to(1200);
    h.video.advance_to(1200);
    EXPECT_EQ(h.algo->get_drift_info().drift_ms, 0);
}

TEST(EngineObserver, PauseFreezesDriftAgainstClock) {
    SyncHarness h;
    h.finish_calibration();
    h.audio.advance_to(5000);
    h.video.advance_to(5000);

    h.observer->set_paused(true);
    h.clock.advance(100000);
    h.audio.advance_to(6000);
    h.video.advance_to(9000);
    EXPECT_EQ(h.algo->get_drift_info().audio_pts, 5000ULL);

    h.observer->set_paused(false);
    h.audio.advance_to(6000);
    EXPECT_EQ(h.algo->get_drift_info().audio_pts, 6000ULL);
}

TEST(EngineObserver, NoCallbackAfterDetachAndRepeatedAttach) {
    SyncHarness h;
    h.audio.advance_to(1000);
    ASSERT_EQ(h.algo->get_drift_info().audio_pts, 1000ULL);

    h.video.set_frame_observer(nullptr);
    h.video.advance_to(2000);
    EXPECT_EQ(h.algo->get_drift_info().video_pts, 0ULL);

    for (int i = 0; i < 3; ++i) {
        h.video.set_frame_observer(h.observer.get());
        h.video.advance_to(3000);
        h.video.set_frame_observer(nullptr);
    }
    EXPECT_EQ(h.algo->get_drift_info().video_pts, 3000ULL);
}

/* ==================== #90 决策闭环 ==================== */

TEST(SyncAlgorithm, AheadThresholdBoundaries) {
    auto algo = Prism::Business::create_sync_algorithm();
    EXPECT_EQ(algo->calibrate(1000, 1030), SyncAction::RENDER); // == +30 未越界
    EXPECT_EQ(algo->calibrate(1000, 1031), SyncAction::WAIT);   // +31 越界
}

TEST(SyncAlgorithm, BehindThresholdBoundaries) {
    auto algo = Prism::Business::create_sync_algorithm();
    EXPECT_EQ(algo->calibrate(1000, 950), SyncAction::RENDER); // == -50 未越界
    EXPECT_EQ(algo->calibrate(1000, 949), SyncAction::DROP);   // -51 越界
}

TEST(EngineObserver, DecisionCountsMatchSyncAction) {
    DecisionLoopReport report = run_decision_loop();
    EXPECT_EQ(report.presented, 4); // 校准期 2 帧 + 收尾 2 个 RENDER
    EXPECT_EQ(report.waited, 1);
    EXPECT_EQ(report.dropped, 1);
}

TEST(EngineObserver, DecisionLoopStableAcrossThreeRuns) {
    DecisionLoopReport a = run_decision_loop();
    DecisionLoopReport b = run_decision_loop();
    DecisionLoopReport c = run_decision_loop();

    for (const auto* r : {&b, &c}) {
        EXPECT_EQ(a.drift_ms, r->drift_ms);
        EXPECT_EQ(a.presented, r->presented);
        EXPECT_EQ(a.waited, r->waited);
        EXPECT_EQ(a.dropped, r->dropped);
        EXPECT_EQ(a.inter_client_skew_ms, r->inter_client_skew_ms);
    }
}

TEST(SyncReport, DriftAndInterClientSkewAreSeparateFields) {
    DecisionLoopReport report = run_decision_loop();
    // 本票只产出 drift；跨客户端偏差属于其它票，报告字段独立且不被 drift 污染。
    EXPECT_NE(report.drift_ms, 0);
    EXPECT_EQ(report.inter_client_skew_ms, 0);
}

} // namespace
