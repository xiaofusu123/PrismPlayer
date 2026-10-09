project(client-engine)

message(STATUS "${PROJECT_NAME} is building")

find_package(FFMPEG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(soundtouch REQUIRED)
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
        $<$<TARGET_EXISTS:client-adapter>:client-adapter>

    PRIVATE
        ${FFMPEG_LIBRARIES}
        glfw
        Vulkan::Vulkan
        SoundTouch::SoundTouch
        spdlog::spdlog
    )
else()
    message(WARNING "No source files found for ${PROJECT_NAME}")
endif()
