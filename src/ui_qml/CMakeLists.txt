# Qt QML GUI Application
qt_add_executable(ir_gui
    main.cpp
    AppController.h
    AppController.cpp
)

target_include_directories(ir_gui PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(ir_gui PRIVATE
    IR_QML_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(ir_gui PRIVATE
    ir_core
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
)

# Android deployment and debugging configuration for Qt Creator
if(ANDROID)
    if(DEFINED ANDROID_ABI)
        set_target_properties(ir_gui PROPERTIES QT_ANDROID_ABIS "${ANDROID_ABI}")
    elseif(DEFINED CMAKE_ANDROID_ARCH_ABI)
        set_target_properties(ir_gui PROPERTIES QT_ANDROID_ABIS "${CMAKE_ANDROID_ARCH_ABI}")
    else()
        set_target_properties(ir_gui PROPERTIES QT_ANDROID_ABIS "arm64-v8a")
    endif()

    set_target_properties(ir_gui PROPERTIES
        QT_ANDROID_PACKAGE_NAME "org.ir.transceiver"
        QT_ANDROID_PACKAGE_VERSION_CODE 1
        QT_ANDROID_PACKAGE_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_APP_NAME "IR Transceiver"
        QT_ANDROID_MIN_SDK_VERSION "23"
        QT_ANDROID_TARGET_SDK_VERSION "34"
        QT_ANDROID_PERMISSIONS "android.permission.INTERNET;android.permission.ACCESS_NETWORK_STATE;android.permission.ACCESS_WIFI_STATE"
    )
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/android")
        set_target_properties(ir_gui PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        )
    endif()
endif()

# Register QML files so IDEs (Qt Creator) show them in the project tree and RCC bundles them
qt_add_qml_module(ir_gui
    URI IR
    VERSION 1.0
    QML_FILES
        main.qml
        TopBar.qml
        ManualAddPopup.qml
        CapturePopup.qml
        TriggerPopup.qml
        IconButton.qml
    RESOURCES
        qtquickcontrols2.conf
        assets/icon.png
        assets/icon_256.png
        assets/icon_128.png
        assets/icon_64.png
)

qt_add_resources(ir_gui "qtquickcontrols_conf"
    PREFIX "/"
    FILES
        qtquickcontrols2.conf
)


