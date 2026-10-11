# Single Unified Test Executable (Testing both ir_core engine and ir_api REST service)
add_executable(test_all
    test_all.cpp
)

target_include_directories(test_all PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/api
    ${CMAKE_SOURCE_DIR}/src/ui_tui
)

target_link_libraries(test_all PRIVATE
    ir_api
)

# Register individual test cases with CTest
add_test(NAME core_ring_buffer           COMMAND test_all ring_buffer)
add_test(NAME core_pulse_parsing         COMMAND test_all pulse_parsing)
add_test(NAME core_agc_leader_fix        COMMAND test_all agc_leader)
add_test(NAME core_tiqiaa_payload        COMMAND test_all tiqiaa_payload)
add_test(NAME core_multi_chunk           COMMAND test_all multi_chunk)
add_test(NAME core_nec_codec             COMMAND test_all nec_codec)
add_test(NAME core_transceiver_lifecycle COMMAND test_all transceiver)
add_test(NAME core_signal_storage        COMMAND test_all storage_matching)
add_test(NAME api_rest_endpoints         COMMAND test_all rest_api)
