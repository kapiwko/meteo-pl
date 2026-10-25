qt_add_executable(meteo-pl main.cpp)
qt_add_qml_module(meteo-pl
    URI pl.meteo.app
    VERSION 1.0
    NO_IMPORT_SCAN
    QML_FILES Main.qml
)
target_link_libraries(meteo-pl PRIVATE meteo_ui Qt6::Quick Qt6::QuickControls2)
set_target_properties(meteo-pl PROPERTIES INSTALL_RPATH "$ORIGIN/../${METEO_QML_INSTALL_DIR}")
install(TARGETS meteo-pl RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES pl.meteo.desktop.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
