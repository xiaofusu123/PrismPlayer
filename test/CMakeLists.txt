project(test)

message(STATUS "${PROJECT_NAME} is building")

find_package(GTest REQUIRED)
include(GoogleTest)

add_subdirectory(adapter)
add_subdirectory(engine)
add_subdirectory(business)
add_subdirectory(service)

add_subdirectory(server)
