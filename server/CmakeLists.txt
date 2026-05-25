project(Prism-server VERSION 0.0.1)

message(STATUS "")
message(STATUS "${PROJECT_NAME} is building, version is ${PROJECT_VERSION}")

add_subdirectory(adapter)
add_subdirectory(network)
add_subdirectory(business)

add_executable(${PROJECT_NAME} main.cpp)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        business/include
        adapter/include
        network/include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        server-adapter
        server-network
        server-business
)
