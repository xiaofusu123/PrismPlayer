project(client-adapter)

message(STATUS "${PROJECT_NAME} is building")

find_package(spdlog REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ixwebsocket REQUIRED)

# sqlite3：vcpkg 的端口只导出 unofficial-sqlite3 CONFIG 包；CMake 自带的 FindSQLite3
# （3.14+，发行版包常用）提供 SQLite3::SQLite3。两条路都接上，统一到 PRISM_SQLITE_TARGET，
# 免得换平台时卡在「找不到包」这种配置期错误上。
find_package(unofficial-sqlite3 CONFIG QUIET)
if(TARGET unofficial::sqlite3::sqlite3)
    set(PRISM_SQLITE_TARGET unofficial::sqlite3::sqlite3)
else()
    find_package(SQLite3 REQUIRED)
    set(PRISM_SQLITE_TARGET SQLite::SQLite3)
endif()
message(STATUS "sqlite3 target: ${PRISM_SQLITE_TARGET}")

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
        # 这三者只出现在 internal/ 头与 .cpp 里，公共头不暴露它们的类型。静态库的
        # PRIVATE 依赖会以 $<LINK_ONLY:...> 继续传播到最终链接，上层不必重复 find_package。
        ${PRISM_SQLITE_TARGET}
        OpenSSL::Crypto
        ixwebsocket::ixwebsocket
        spdlog::spdlog_header_only
    )
else()
    message(WARNING "No source files found for ${PROJECT_NAME}")
endif()
