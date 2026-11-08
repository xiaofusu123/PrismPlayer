#include <gtest/gtest.h>

#include "EngineObserver.h"
#include "SyncFactory.h"

namespace {

TEST(SyncFactory, CreatesAllComponents) {
    EXPECT_NE(Prism::Business::create_playback_state_machine(), nullptr);
    EXPECT_NE(Prism::Business::create_sync_algorithm(), nullptr);
    EXPECT_NE(Prism::Business::create_command_dispatcher(), nullptr);
    EXPECT_NE(Prism::Business::create_engine_observer(), nullptr);
}

TEST(SyncFactory, ObserverUpcastsToEngineObserver) {
    auto observer = Prism::Business::create_engine_observer();
    ASSERT_NE(observer, nullptr);

    Prism::Engine::EngineObserver* engine_observer = observer.get();
    EXPECT_NE(engine_observer, nullptr);
}

} // namespace
