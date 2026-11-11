project(server-adapter)

message(STATUS "${PROJECT_NAME} is building")

find_package(spdlog REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(unofficial-libmysql QUIET)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC)

# libmysql 仅 Linux 可用（x64-mingw-dynamic 不支持）；未启用 mysql feature 时不编译 MySQL 实现。
if(NOT TARGET unofficial::libmysql::libmysql)
    list(REMOVE_ITEM SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/MySqlDbAdapter.cpp)
endif()

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
    PRIVATE
        spdlog::spdlog_header_only
        OpenSSL::Crypto
    )
    if(TARGET unofficial::libmysql::libmysql)
        target_compile_definitions(${PROJECT_NAME} PRIVATE PRISM_HAS_MYSQL)
        target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::libmysql::libmysql)
    endif()
else()
    message(WARNING "No source files found for ${PROJECT_NAME}")
endif()
