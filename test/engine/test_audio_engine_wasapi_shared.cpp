#include <gtest/gtest.h>

#include "AudioEngine.h"
#include "AudioEngineFactory.h"
#include "AudioEngineWasapiSharedFactory.h"

namespace {

using Prism::Engine::AudioEngineWasapiSharedFactory;

TEST(AudioEngineWasapiShared, FactoryCreatesEngine) {
    AudioEngineWasapiSharedFactory factory;
    auto engine = factory.create_audio_engine();
    ASSERT_NE(engine, nullptr);
}

TEST(AudioEngineWasapiShared, LifecycleReturnsTrue) {
    AudioEngineWasapiSharedFactory factory;
    auto engine = factory.create_audio_engine();
    ASSERT_NE(engine, nullptr);

    EXPECT_TRUE(engine->init());
    EXPECT_TRUE(engine->play());
    EXPECT_TRUE(engine->pause());
    EXPECT_TRUE(engine->set_play_speed(1.0F));
    EXPECT_TRUE(engine->seek(std::chrono::milliseconds{0}));
    EXPECT_TRUE(engine->close());
}

TEST(AudioEngineWasapiShared, SyncInfoStartsAtZero) {
    AudioEngineWasapiSharedFactory factory;
    auto engine = factory.create_audio_engine();
    ASSERT_NE(engine, nullptr);

    Prism::Engine::AudioSyncInfo info = engine->get_sync_info();
    EXPECT_EQ(info.current_pts.load().count(), 0);
    EXPECT_EQ(info.next_pts.load().count(), 0);
    EXPECT_EQ(info.start_time.load().count(), 0);
    EXPECT_EQ(info.duration.load().count(), 0);
}

} // namespace
