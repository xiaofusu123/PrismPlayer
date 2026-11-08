#include <gtest/gtest.h>

#include "AudioEngineFactory.h"
#include "EngineObserver.h"
#include "SyncFactory.h"
#include "VideoEngineFactory.h"

namespace {

class FakeAudioEngine : public Prism::Engine::AudioEngine {
  public:
    [[nodiscard]] Prism::Engine::EngineObserver* registered_observer() const { return observer_; }

    bool init() override { return true; }
    bool play() override { return true; }
    bool pause() override { return true; }
    bool close() override { return true; }
    bool set_play_speed() override { return true; }
    bool seek(uint64_t, int) override { return true; }
    Prism::Engine::AudioSyncInfo get_sync_info() override { return {}; }
    void set_observer(Prism::Engine::EngineObserver* observer) override { observer_ = observer; }

  private:
    Prism::Engine::EngineObserver* observer_{nullptr};
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
    [[nodiscard]] Prism::Engine::EngineObserver* registered_observer() const { return observer_; }

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
        auto engine = std::make_unique<FakeVideoEngine>();
        last_engine_ = engine.get();
        return engine;
    }
    [[nodiscard]] FakeVideoEngine* last_engine() const { return last_engine_; }

  private:
    FakeVideoEngine* last_engine_{nullptr};
};

TEST(CommandDispatcher, RegistersObserverOnInitializeEngines) {
    auto dispatcher = Prism::Business::create_command_dispatcher();
    auto observer = Prism::Business::create_engine_observer();
    ASSERT_NE(dispatcher, nullptr);
    ASSERT_NE(observer, nullptr);

    dispatcher->set_observer(observer.get());

    FakeAudioFactory audio_factory;
    FakeVideoFactory video_factory;
    ASSERT_TRUE(dispatcher->initialize_engines(&audio_factory, &video_factory, true));

    ASSERT_NE(audio_factory.last_engine(), nullptr);
    ASSERT_NE(video_factory.last_engine(), nullptr);
    EXPECT_EQ(audio_factory.last_engine()->registered_observer(), observer.get());
    EXPECT_EQ(video_factory.last_engine()->registered_observer(), observer.get());
}

TEST(CommandDispatcher, RegistersObserverOnInjectedEngine) {
    auto dispatcher = Prism::Business::create_command_dispatcher();
    auto observer = Prism::Business::create_engine_observer();
    ASSERT_NE(dispatcher, nullptr);
    ASSERT_NE(observer, nullptr);

    dispatcher->set_observer(observer.get());

    FakeAudioEngine audio;
    dispatcher->set_audio_engine(&audio);
    EXPECT_EQ(audio.registered_observer(), observer.get());

    FakeVideoEngine video;
    dispatcher->set_video_engine(&video);
    EXPECT_EQ(video.registered_observer(), observer.get());
}

} // namespace
