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
