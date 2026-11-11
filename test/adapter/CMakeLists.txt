project(test-adapter)

message(STATUS "${PROJECT_NAME} is building")

file(GLOB SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

if(SRC)
    message(STATUS "The ${PROJECT_NAME} of source file include:")
    foreach(file ${SRC})
        get_filename_component(name ${file} NAME)
        message(STATUS "${name}")
    endforeach()
    message(STATUS "")

    add_executable(${PROJECT_NAME} ${SRC})

    # 测的是具体实现（SQLiteDBAdapter / OpenSSLCryptoAdapter），它们的头在 internal/ 下，
    # 属于 client-adapter 的 PRIVATE include 目录，测试目标要自己补一次。
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/client/adapter/internal
    )

    target_link_libraries(${PROJECT_NAME}
        PUBLIC
            GTest::gtest_main
            $<$<TARGET_EXISTS:client-adapter>:client-adapter>
    )

    if(WIN32 AND TARGET client-adapter)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${PROJECT_NAME}>
                    $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
            COMMAND_EXPAND_LISTS)
    endif()

    gtest_discover_tests(${PROJECT_NAME} PROPERTIES LABELS "client-adapter")
else()
    message(WARNING "No source files found for ${PROJECT_NAME}")
endif()
