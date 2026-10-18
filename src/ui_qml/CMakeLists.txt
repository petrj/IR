# Qt QML GUI Application
add_executable(ir_gui
    main.cpp
    AppController.h
    AppController.cpp
)

target_include_directories(ir_gui PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/ui_tui
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
)

# Register QML files so IDEs (Qt Creator) show them in the project tree
if (TARGET ir_gui)
    qt_add_qml_module(ir_gui
        URI IR
        VERSION 1.0
        QML_FILES
            main.qml
            TopBar.qml
            ManualAddPopup.qml
            CapturePopup.qml
            TriggerPopup.qml
    )
endif()
