project(client-engine)

message(STATUS "${PROJECT_NAME} is building")

if(POLICY CMP0174)
    cmake_policy(SET CMP0174 NEW)
endif()

find_package(FFMPEG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
# SoundTouch 的 vcpkg 包装出来的是 SoundTouchConfig.cmake（大小写就是这个），所以包名必须写
# SoundTouch：CMake 的 config 模式按「<包名>Config.cmake」和「<包名小写>-config.cmake」精确匹配。
# 写成小写 soundtouch 只在大小写不敏感的文件系统上碰巧能命中（Windows / WSL 的 /mnt/d 都是），
# 干净的 Linux（ext4）上直接 find_package 失败 —— #99 的 CI 首次真跑就是这么红的。
find_package(SoundTouch REQUIRED)
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
