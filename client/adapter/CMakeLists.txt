project(client-adapter)

message(STATUS "${PROJECT_NAME} is building")

find_package(spdlog REQUIRED)
# 包名是 nlohmann_json（下划线），与 vcpkg.json 里的 nlohmann-json（连字符）不是同一个写法；
# 目标名同理用 nlohmann_json::nlohmann_json。JSONConfigAdapter 的头文件里
# 直接持有 nlohmann::json 成员，所以按 PUBLIC 传递出去。
find_package(nlohmann_json CONFIG REQUIRED)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC)

if(SRC)
    message(STATUS "The ${PROJECT_NAME} of source file include:")
    foreach(file ${SRC})
        get_filename_component(name ${file} NAME)
        message(STATUS "${name}")
    endforeach()
    message(STATUS "")

    add_library(${PROJECT_NAME} STATIC ${SRC})
    target_include_directories(${PROJECT_NAME}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/internal
    )

    target_link_libraries(${PROJECT_NAME}
    PUBLIC
        nlohmann_json::nlohmann_json

    PRIVATE
        spdlog::spdlog_header_only
    )
else()
    message(WARNING "No source files found for ${PROJECT_NAME}")
endif()
