project(test-acceptance)

message(STATUS "${PROJECT_NAME} is building")

# 验收用例：main.cpp（自定义入口，写证据报告）+ test_*.cpp + support/（假引擎与报告写入）
file(GLOB_RECURSE SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

if(SRC)
    message(STATUS "The ${PROJECT_NAME} of source file include:")
    foreach(file ${SRC})
        get_filename_component(name ${file} NAME)
        message(STATUS "${name}")
    endforeach()
    message(STATUS "")

    add_executable(${PROJECT_NAME} ${SRC})

    target_include_directories(${PROJECT_NAME}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/client/engine/include
    )

    # 自定义 main（写报告），所以只链 GTest::gtest，不链 gtest_main。
    # client-engine 也直接链一份：client-service.dll 在运行时的引擎符号要靠同目录的
    # libclient-engine.dll，而 TARGET_RUNTIME_DLLS 不会展开 client-service 的私有依赖。
    target_link_libraries(${PROJECT_NAME}
        PUBLIC
            GTest::gtest
            $<$<TARGET_EXISTS:client-service>:client-service>
            $<$<TARGET_EXISTS:client-engine>:client-engine>
    )

    if(WIN32 AND TARGET client-service)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${PROJECT_NAME}>
                    $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
            COMMAND_EXPAND_LISTS)
    endif()

    # 默认标签 e2e；e2e-multi / perf / perf-long 为保留标签，
    # 由 CMakePresets.json 的 testPresets filter 在默认运行时排除。
    gtest_discover_tests(${PROJECT_NAME} PROPERTIES LABELS "e2e")
else()
    message(WARNING "No source files found for ${PROJECT_NAME}")
endif()
