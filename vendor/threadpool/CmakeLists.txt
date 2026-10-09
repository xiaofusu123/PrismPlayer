cmake_minimum_required(VERSION 3.20)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(ThreadPool
    VERSION 1.0.0
    DESCRIPTION "实现异步和多线程的线程池"
)

message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC)

add_library(${PROJECT_NAME}_static STATIC ${SRC})
add_library(${PROJECT_NAME}_shared SHARED ${SRC})

target_include_directories(${PROJECT_NAME}_static
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(${PROJECT_NAME}_shared
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(TEST true)

if (TEST)
    enable_testing()
    add_subdirectory(test)
endif()
