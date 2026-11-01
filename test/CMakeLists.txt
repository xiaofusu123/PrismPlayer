project(test)

message(STATUS "${PROJECT_NAME} is building")

find_package(GTest REQUIRED)
include(GoogleTest)

add_subdirectory(engine)
