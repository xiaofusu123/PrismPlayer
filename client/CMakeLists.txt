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

if(NOT TARGET client-service)
    message(FATAL_ERROR "The target(client-service) doesn't exist: client/service/src is empty")
endif()

if(WIN32)
    target_compile_definitions(client-service PRIVATE _PRISM_EXPORTS)
else()
    set_target_properties(client-service PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    
    target_compile_options(client-service PRIVATE -fvisibility=hidden)
endif()
