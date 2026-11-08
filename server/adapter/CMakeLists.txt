project(server-adapter)

message(STATUS "${PROJECT_NAME} is building")

option(PRISM_SERVER_WITH_MYSQL "Build the MySQL DB adapter (requires libmariadb)" OFF)

find_package(spdlog REQUIRED)
find_package(OpenSSL REQUIRED)

if(PRISM_SERVER_WITH_MYSQL)
    find_package(unofficial-libmariadb CONFIG REQUIRED)
endif()

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC)

if(NOT PRISM_SERVER_WITH_MYSQL)
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

    if(PRISM_SERVER_WITH_MYSQL)
        target_compile_definitions(${PROJECT_NAME} PRIVATE PRISM_SERVER_WITH_MYSQL)
        target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::libmariadb)
    endif()
else()
    message(WARNING "No source files found for ${PROJECT_NAME}")
endif()
