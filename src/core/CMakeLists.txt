# IR Core Static Library
set(CORE_SOURCES
    README.md
    RingBuffer.h
    IrProtocol.h
    IrProtocol.cpp
    SignalStorage.h
    SignalStorage.cpp
)

if((TARGET PkgConfig::LIBUSB OR LIBUSB_FOUND) AND NOT ANDROID)
    list(APPEND CORE_SOURCES
        IrUsbTransceiver.h
        IrUsbTransceiver.cpp
    )
endif()

add_library(ir_core STATIC ${CORE_SOURCES})

target_include_directories(ir_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(ir_core PUBLIC
    Threads::Threads
    nlohmann_json::nlohmann_json
)

if(TARGET PkgConfig::LIBUSB AND NOT ANDROID)
    target_link_libraries(ir_core PUBLIC PkgConfig::LIBUSB)
    target_compile_definitions(ir_core PUBLIC IR_HAS_LIBUSB=1)
elseif(LIBUSB_FOUND AND NOT ANDROID)
    target_link_libraries(ir_core PUBLIC ${LIBUSB_LIBRARIES})
    target_include_directories(ir_core PUBLIC ${LIBUSB_INCLUDE_DIRS})
    target_compile_definitions(ir_core PUBLIC IR_HAS_LIBUSB=1)
else()
    target_compile_definitions(ir_core PUBLIC IR_HAS_LIBUSB=0)
endif()

