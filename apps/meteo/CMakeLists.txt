find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
qt_add_executable(meteo-pl WIN32 MACOSX_BUNDLE main.cpp)
target_compile_definitions(meteo-pl PRIVATE METEO_VERSION="${PROJECT_VERSION}")
qt_add_resources(meteo-pl "application_icon" PREFIX "/icons"
    BASE "${PROJECT_SOURCE_DIR}/integrations/plasma/package/contents/images"
    FILES "${PROJECT_SOURCE_DIR}/integrations/plasma/package/contents/images/meteo.svg")
qt_add_qml_module(meteo-pl
    URI pl.meteo.app
    VERSION 1.0
    NO_IMPORT_SCAN
    QML_FILES Main.qml Mobile.qml ForecastWindow.qml ForecastSession.qml
)
target_link_libraries(meteo-pl PRIVATE meteo_ui Qt6::Quick Qt6::QuickControls2 Qt6::Widgets)
if(WIN32 OR APPLE OR ANDROID)
    target_link_libraries(meteo-pl PRIVATE KF6::IconThemes)
    target_compile_definitions(meteo-pl PRIVATE METEO_BUNDLED_ICONS)
endif()
if(APPLE)
    set_target_properties(meteo-pl PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/packaging/macos/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "pl.meteo.desktop"
        MACOSX_BUNDLE_BUNDLE_NAME "Meteo"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        INSTALL_RPATH "@executable_path/../Frameworks;@executable_path/../../../../lib")
elseif(NOT WIN32 AND NOT ANDROID)
    set_target_properties(meteo-pl PROPERTIES INSTALL_RPATH "$ORIGIN/../${METEO_QML_INSTALL_DIR}")
endif()
if(WIN32 OR APPLE)
    include(ECMAddAppIcon)
    ecm_add_app_icon(meteo-pl ICONS
        "${PROJECT_SOURCE_DIR}/packaging/icons/32-apps-meteo-pl.png"
        "${PROJECT_SOURCE_DIR}/packaging/icons/48-apps-meteo-pl.png"
        "${PROJECT_SOURCE_DIR}/packaging/icons/128-apps-meteo-pl.png"
        "${PROJECT_SOURCE_DIR}/packaging/icons/256-apps-meteo-pl.png"
        "${PROJECT_SOURCE_DIR}/packaging/icons/512-apps-meteo-pl.png")
endif()
install(TARGETS meteo-pl BUNDLE DESTINATION "bin" RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
if(NOT ANDROID)
    install(FILES pl.meteo.desktop.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
endif()
if(ANDROID)
    qt_add_resources(meteo-pl "android_controls" PREFIX "/"
        BASE "${PROJECT_SOURCE_DIR}/packaging/android"
        FILES "${PROJECT_SOURCE_DIR}/packaging/android/qtquickcontrols2.conf")
    include("${PROJECT_SOURCE_DIR}/packaging/android/BreezeIcons.cmake")
    include(ECMAddAndroidApk)
    math(EXPR METEO_ANDROID_VERSION_CODE "${PROJECT_VERSION_MAJOR} * 1000000 + ${PROJECT_VERSION_MINOR} * 1000 + ${PROJECT_VERSION_PATCH}")
    set(METEO_ANDROID_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/android")
    configure_file("${PROJECT_SOURCE_DIR}/packaging/android/AndroidManifest.xml.in"
        "${METEO_ANDROID_PACKAGE_DIR}/AndroidManifest.xml" @ONLY)
    file(COPY "${PROJECT_SOURCE_DIR}/packaging/android/res" DESTINATION "${METEO_ANDROID_PACKAGE_DIR}")
    ecm_add_android_apk(meteo-pl ANDROID_DIR "${METEO_ANDROID_PACKAGE_DIR}")
endif()
