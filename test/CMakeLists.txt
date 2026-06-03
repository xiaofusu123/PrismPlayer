project(prism-tests)

message(STATUS "${PROJECT_NAME} is building")

find_package(spdlog CONFIG REQUIRED)

# ========== sync_types_test ==========
add_executable(sync_types_test
    sync_types_test.cpp
)
target_include_directories(sync_types_test PRIVATE
    ${CMAKE_SOURCE_DIR}/client/common
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/include
)
target_link_libraries(sync_types_test PRIVATE gtest_main)
add_test(NAME sync_types_test COMMAND sync_types_test)

# ========== av_synchronizer_test ==========
add_executable(av_synchronizer_test
    av_synchronizer_test.cpp
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/src/av_synchronizer.cpp
)
target_include_directories(av_synchronizer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/client/common
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/include
    ${CMAKE_SOURCE_DIR}/client/business/av_sync/Impl
)
target_link_libraries(av_synchronizer_test PRIVATE gtest_main spdlog::spdlog)
add_test(NAME av_synchronizer_test COMMAND av_synchronizer_test)
