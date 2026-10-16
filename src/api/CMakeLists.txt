# IR Embedded REST API Library
add_library(ir_api STATIC
    README.md
    HttpServerModule.h
    HttpServerModule.cpp
)

target_include_directories(ir_api PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/core>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/ui_tui>
)

target_link_libraries(ir_api PUBLIC
    ir_core
    httplib::httplib
    nlohmann_json::nlohmann_json
)

# Standalone REST API Server Executable
add_executable(ir_api_server
    main_api.cpp
)

target_link_libraries(ir_api_server PRIVATE
    ir_api
)
