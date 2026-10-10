# Testing Console Application (TUI / CLI for reading, saving, and transmitting IR signals)
add_executable(ir_tui
    main_tui.cpp
    TuiController.h
    TuiController.cpp
    SignalStorage.h
    SignalStorage.cpp
)

target_include_directories(ir_tui PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ir_tui PRIVATE
    ir_core
    httplib::httplib
    nlohmann_json::nlohmann_json
)
