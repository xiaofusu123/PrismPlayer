cmake_minimum_required(VERSION 3.20)

project(PrismPlayer)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")

message(STATUS "Current mode is ${CMAKE_BUILD_TYPE}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(PRISM_DEBUG)
else()
    add_compile_definitions(PRISM_RELEASE)
endif()
message(STATUS "")

option(PRISM_COVERAGE "Instrument project targets for LLVM source-based coverage" OFF)
if(PRISM_COVERAGE)
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)

    if(WIN32)
        add_link_options(-fuse-ld=lld)
    endif()
    message(STATUS "Coverage instrumentation: ON (LLVM source-based)")
endif()

add_subdirectory(client)
add_subdirectory(server)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    enable_testing()
    add_subdirectory(test)
endif()
