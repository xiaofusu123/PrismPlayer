project(client-adapter)

message(STATUS "${PROJECT_NAME} is building")

find_package(spdlog REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(ixwebsocket REQUIRED)

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

    # sqlite3 / OpenSSL / ixwebsocket 的类型只出现在 internal/ 与 .cpp 里，
    # 因此一律 PRIVATE：PUBLIC 头文件不暴露它们。
    # client-adapter 是 STATIC，PRIVATE 依赖会以 $<LINK_ONLY:...> 继续传播到最终链接，
    # 所以 client-engine / client-service 不必重复 find_package 这三个。
    target_link_libraries(${PROJECT_NAME}
    PUBLIC
        nlohmann_json::nlohmann_json

    PRIVATE
        OpenSSL::Crypto
        spdlog::spdlog_header_only
        unofficial::sqlite3::sqlite3
        ixwebsocket::ixwebsocket
    )
else()
    message(WARNING "No source files found for ${PROJECT_NAME}")
endif()
