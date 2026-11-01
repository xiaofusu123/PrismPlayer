#include <gtest/gtest.h>

#include "VideoEngine.h"
#include "VideoEngineFactory.h"
#include "VideoEngineVulkanFactory.h"

namespace {

using Prism::Engine::VideoEngineVulkanFactory;

TEST(VideoEngineVulkan, FactoryCreatesEngine) {
    VideoEngineVulkanFactory factory;
    auto engine = factory.create_video_engine();
    ASSERT_NE(engine, nullptr);
}

TEST(VideoEngineVulkan, LifecycleReturnsTrue) {
    VideoEngineVulkanFactory factory;
    auto engine = factory.create_video_engine();
    ASSERT_NE(engine, nullptr);

    EXPECT_TRUE(engine->init());
    EXPECT_TRUE(engine->play());
    EXPECT_TRUE(engine->pause());
    EXPECT_TRUE(engine->set_play_speed(1.0F));
    EXPECT_TRUE(engine->seek(0, 0));
    EXPECT_TRUE(engine->close());
}

TEST(VideoEngineVulkan, SyncInfoAndRenderResultStartEmpty) {
    VideoEngineVulkanFactory factory;
    auto engine = factory.create_video_engine();
    ASSERT_NE(engine, nullptr);

    Prism::Engine::VideoSyncInfo info = engine->get_sync_info();
    EXPECT_EQ(info.current_pts.load(), 0U);
    EXPECT_EQ(info.next_pts.load(), 0U);
    EXPECT_EQ(info.start_time.load(), 0U);
    EXPECT_EQ(info.draution.load(), 0U);

    Prism::Engine::RenderMetadata metadata = engine->get_render_result();
    EXPECT_EQ(metadata.handle, RenderResultHandle{});
    EXPECT_FALSE(metadata.valid);
    EXPECT_EQ(metadata.width, 0U);
    EXPECT_EQ(metadata.height, 0U);
    EXPECT_EQ(metadata.format, 0U);
    EXPECT_EQ(metadata.timestamp, 0U);
}

} // namespace
