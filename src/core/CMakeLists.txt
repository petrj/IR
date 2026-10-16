# IR Core Static Library
add_library(ir_core STATIC
    README.md
    RingBuffer.h
    IrProtocol.h
    IrProtocol.cpp
    IrUsbTransceiver.h
    IrUsbTransceiver.cpp
    SignalStorage.h
    SignalStorage.cpp
)

target_include_directories(ir_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(ir_core PUBLIC
    PkgConfig::LIBUSB
    Threads::Threads
)
