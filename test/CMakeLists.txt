project(prism-test)

include(GoogleTest)

find_package(GTest CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# Common include directories for all test targets
set(TEST_COMMON_INCLUDES
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/include
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/Impl
    ${CMAKE_SOURCE_DIR}/client/engine/include
)

# ---- PlaybackStateMachine tests ----
add_executable(test_playback_state_machine
    test_playback_state_machine.cpp
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/src/PlaybackStateMachine.cpp
)
target_include_directories(test_playback_state_machine PRIVATE ${TEST_COMMON_INCLUDES})
target_link_libraries(test_playback_state_machine PRIVATE GTest::gtest_main spdlog::spdlog)
gtest_discover_tests(test_playback_state_machine)

# ---- SyncAlgorithm tests ----
add_executable(test_sync_algorithm
    test_sync_algorithm.cpp
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/src/SyncAlgorithm.cpp
)
target_include_directories(test_sync_algorithm PRIVATE ${TEST_COMMON_INCLUDES})
target_link_libraries(test_sync_algorithm PRIVATE GTest::gtest_main spdlog::spdlog)
gtest_discover_tests(test_sync_algorithm)

# ---- SyncFactory tests ----
add_executable(test_sync_factory
    test_sync_factory.cpp
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/src/SyncFactory.cpp
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/src/PlaybackStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/src/SyncAlgorithm.cpp
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/src/CommandDispatcher.cpp
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/src/EngineObserver.cpp
)
target_include_directories(test_sync_factory PRIVATE ${TEST_COMMON_INCLUDES})
target_link_libraries(test_sync_factory PRIVATE GTest::gtest_main spdlog::spdlog)
gtest_discover_tests(test_sync_factory)

# ---- EngineObserver tests ----
add_executable(test_engine_observer
    test_engine_observer.cpp
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/src/EngineObserver.cpp
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/src/PlaybackStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/src/SyncAlgorithm.cpp
)
target_include_directories(test_engine_observer PRIVATE ${TEST_COMMON_INCLUDES})
target_link_libraries(test_engine_observer PRIVATE GTest::gtest_main spdlog::spdlog)
gtest_discover_tests(test_engine_observer)
