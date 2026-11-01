project(server-network)

message(STATUS "${PROJECT_NAME} is building")

find_package(asio REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(ixwebsocket REQUIRED)
find_package(spdlog REQUIRED)

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
        $<$<TARGET_EXISTS:server-business>:server-business>

    PRIVATE
        asio::asio
        ixwebsocket::ixwebsocket
        nlohmann_json
        spdlog::spdlog_header_only
    )

    if(WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE bcrypt)
    endif()
else()
    message(WARNING "No source files found for ${PROJECT_NAME}")
endif()
