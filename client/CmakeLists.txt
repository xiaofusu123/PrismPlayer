project(Prism-client VERSION 0.0.1)

message(STATUS "${PROJECT_NAME} is building, version is ${PROJECT_VERSION}")

add_library(common INTERFACE)
target_include_directories(common
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/common
)

add_subdirectory(adapter)
add_subdirectory(engine)
add_subdirectory(business)
add_subdirectory(service)

if(WIN32)
    target_compile_definitions(client-service PRIVATE _PRISM_EXPORTS)
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden)
endif()
