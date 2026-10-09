project(client-service)

message(STATUS "${PROJECT_NAME} is building")

find_package(spdlog REQUIRED)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC)

if(SRC)
    message(STATUS "The ${PROJECT_NAME} of source file include:")
    foreach(file ${SRC})
        get_filename_component(name ${file} NAME)
        message(STATUS "${name}")
    endforeach()
    message(STATUS "")

    add_library(${PROJECT_NAME} SHARED ${SRC})
    target_include_directories(${PROJECT_NAME}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/internal
    )

    target_link_libraries(${PROJECT_NAME}
    PUBLIC
        $<$<TARGET_EXISTS:common>:common>

    PRIVATE
        $<$<TARGET_EXISTS:client-av_sync>:client-av_sync>
        $<$<TARGET_EXISTS:client-network>:client-network>
        spdlog::spdlog
    )
else()
    message(WARNING "No source files found for ${PROJECT_NAME}")
endif()
