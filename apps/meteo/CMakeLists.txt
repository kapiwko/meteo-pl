find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
qt_add_executable(meteo-pl main.cpp)
target_compile_definitions(meteo-pl PRIVATE METEO_VERSION="${PROJECT_VERSION}")
qt_add_resources(meteo-pl "application_icon" PREFIX "/icons"
    BASE "${PROJECT_SOURCE_DIR}/integrations/plasma/package/contents/images"
    FILES "${PROJECT_SOURCE_DIR}/integrations/plasma/package/contents/images/meteo.svg")
qt_add_qml_module(meteo-pl
    URI pl.meteo.app
    VERSION 1.0
    NO_IMPORT_SCAN
    QML_FILES Main.qml
)
target_link_libraries(meteo-pl PRIVATE meteo_ui Qt6::Quick Qt6::QuickControls2 Qt6::Widgets)
set_target_properties(meteo-pl PROPERTIES INSTALL_RPATH "$ORIGIN/../${METEO_QML_INSTALL_DIR}")
install(TARGETS meteo-pl RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES pl.meteo.desktop.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
