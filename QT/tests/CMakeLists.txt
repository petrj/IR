# IR Core Unit Test Executable
add_executable(test_ir_core
    test_core.cpp
    ${CMAKE_SOURCE_DIR}/src/ui_tui/SignalStorage.h
    ${CMAKE_SOURCE_DIR}/src/ui_tui/SignalStorage.cpp
)

target_include_directories(test_ir_core PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ui_tui
)

target_link_libraries(test_ir_core PRIVATE
    ir_core
)

# IR REST API Unit Test Executable
add_executable(test_ir_api
    test_api.cpp
)

target_link_libraries(test_ir_api PRIVATE
    ir_api
)

add_test(NAME ir_core_unit_tests COMMAND test_ir_core)
add_test(NAME ir_api_unit_tests COMMAND test_ir_api)
