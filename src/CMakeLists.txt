add_library(meteo_core STATIC
    core/Forecast.cpp core/Forecast.hpp
    core/Locations.cpp core/Locations.hpp
    network/MeteoClient.cpp network/MeteoClient.hpp
    network/JsonRequest.cpp network/JsonRequest.hpp
)
target_include_directories(meteo_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(meteo_core PUBLIC Qt6::Core Qt6::Network)

foreach(component Meteogram ForecastPanel ForecastInspector ForecastHeader WeatherSymbol PlacePicker SavedPlaceSelection CompactForecast LocationEditor TileMap MapTileFrame MapWheelHandler BufferedImage MeteoSettings ForecastSettings MeteoAbout DisplaySettings ReadingSettings ApplicationSettings SeriesLegend SeriesSettings WeatherMap EditorialView ThematicMaps OriginalMeteogram)
    set_source_files_properties(qml/${component}.qml PROPERTIES QT_RESOURCE_ALIAS ${component}.qml)
endforeach()
set_source_files_properties(qml/MeteogramStyle.js PROPERTIES QT_RESOURCE_ALIAS MeteogramStyle.js)
set_source_files_properties(qml/icons/sunrise.svg PROPERTIES QT_RESOURCE_ALIAS icons/sunrise.svg)
set_source_files_properties(qml/icons/sunset.svg PROPERTIES QT_RESOURCE_ALIAS icons/sunset.svg)
configure_file(qml/AppInfo.js.in "${CMAKE_CURRENT_BINARY_DIR}/AppInfo.js" @ONLY)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/AppInfo.js" PROPERTIES QT_RESOURCE_ALIAS AppInfo.js)
qt_add_qml_module(meteo_ui
    SHARED
    URI pl.meteo.desktop
    VERSION 1.0
    SOURCES presentation/WeatherModel.cpp presentation/WeatherModel.hpp
        presentation/ForecastFields.cpp presentation/ForecastFields.hpp
        presentation/ChartConfiguration.cpp presentation/ChartConfiguration.hpp
        presentation/ChartExporter.cpp presentation/ChartExporter.hpp
        presentation/ReadingsExporter.cpp presentation/ReadingsExporter.hpp
        presentation/ExportNotifications.cpp presentation/ExportNotifications.hpp
        presentation/GpuPlot.cpp presentation/GpuPlot.hpp
        presentation/ResponsiveImages.cpp presentation/ResponsiveImages.hpp
        presentation/ImagePreloader.cpp presentation/ImagePreloader.hpp
        presentation/PlaceSearchModel.cpp presentation/PlaceSearchModel.hpp
        presentation/UserPreferences.cpp presentation/UserPreferences.hpp
        presentation/WeatherLayersModel.cpp presentation/WeatherLayersModel.hpp
        presentation/EditorialModel.cpp presentation/EditorialModel.hpp
        presentation/MapImages.cpp presentation/MapImages.hpp
        presentation/LegacyMeteogramModel.cpp presentation/LegacyMeteogramModel.hpp
        network/ImageCache.cpp network/ImageCache.hpp
    RESOURCES qml/icons/sunrise.svg qml/icons/sunset.svg
    QML_FILES "${CMAKE_CURRENT_BINARY_DIR}/AppInfo.js" qml/Meteogram.qml qml/ForecastPanel.qml qml/ForecastInspector.qml qml/ForecastHeader.qml qml/WeatherSymbol.qml qml/PlacePicker.qml qml/SavedPlaceSelection.qml qml/CompactForecast.qml qml/LocationEditor.qml qml/TileMap.qml
        qml/MeteoSettings.qml qml/ForecastSettings.qml qml/MeteoAbout.qml qml/DisplaySettings.qml qml/ReadingSettings.qml qml/ApplicationSettings.qml qml/WeatherMap.qml qml/EditorialView.qml qml/ThematicMaps.qml
        qml/MapTileFrame.qml qml/MapWheelHandler.qml qml/BufferedImage.qml
        qml/OriginalMeteogram.qml qml/MeteogramStyle.js qml/SeriesLegend.qml qml/SeriesSettings.qml
)
target_link_libraries(meteo_ui PUBLIC meteo_core Qt6::Qml PRIVATE Qt6::Quick Qt6::QuickControls2 KF6::Notifications)
target_include_directories(meteo_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/presentation")
target_compile_definitions(meteo_ui PRIVATE METEO_UI_BUILD)
foreach(target meteo_core meteo_ui)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

if(WIN32 OR APPLE)
    set(METEO_QML_DEFAULT "qml/pl/meteo/desktop")
elseif(ANDROID)
    set(METEO_QML_DEFAULT "lib/qml/pl/meteo/desktop")
else()
    set(METEO_QML_DEFAULT "${CMAKE_INSTALL_LIBDIR}/qt6/qml/pl/meteo/desktop")
endif()
set(METEO_QML_INSTALL_DIR "${METEO_QML_DEFAULT}" CACHE PATH "QML module installation directory")
if(APPLE)
    set_target_properties(meteo_ui meteo_uiplugin PROPERTIES INSTALL_RPATH "@loader_path;@loader_path/../../../../lib")
else()
    set_target_properties(meteo_ui meteo_uiplugin PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()
if(WIN32 OR APPLE OR ANDROID)
    install(TARGETS meteo_ui LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
else()
    install(TARGETS meteo_ui LIBRARY DESTINATION "${METEO_QML_INSTALL_DIR}")
endif()
install(TARGETS meteo_uiplugin LIBRARY DESTINATION "${METEO_QML_INSTALL_DIR}" RUNTIME DESTINATION "${METEO_QML_INSTALL_DIR}")
install(FILES
    "${QT_QML_OUTPUT_DIRECTORY}/pl/meteo/desktop/qmldir"
    "${QT_QML_OUTPUT_DIRECTORY}/pl/meteo/desktop/meteo_ui.qmltypes"
    DESTINATION "${METEO_QML_INSTALL_DIR}"
)
