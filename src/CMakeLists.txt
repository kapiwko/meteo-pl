add_library(meteo_core STATIC
    core/Forecast.cpp core/Forecast.hpp
    core/Locations.cpp core/Locations.hpp
    network/MeteoClient.cpp network/MeteoClient.hpp
    network/JsonRequest.cpp network/JsonRequest.hpp
)
target_include_directories(meteo_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(meteo_core PUBLIC Qt6::Core Qt6::Network)
target_compile_options(meteo_core PRIVATE -Wall -Wextra -Wpedantic)

foreach(component Meteogram ForecastPanel ForecastInspector ForecastHeader CompactForecast LocationEditor TileMap MapTileFrame MapWheelHandler BufferedImage MeteoSettings MeteoAbout DisplaySettings SeriesLegend SeriesSettings WeatherMap EditorialView ThematicMaps OriginalMeteogram)
    set_source_files_properties(qml/${component}.qml PROPERTIES QT_RESOURCE_ALIAS ${component}.qml)
endforeach()
set_source_files_properties(qml/MeteogramStyle.js PROPERTIES QT_RESOURCE_ALIAS MeteogramStyle.js)
qt_add_qml_module(meteo_ui
    SHARED
    URI pl.meteo.desktop
    VERSION 1.0
    NO_PLUGIN_OPTIONAL
    SOURCES presentation/WeatherModel.cpp presentation/WeatherModel.hpp
        presentation/ForecastFields.cpp presentation/ForecastFields.hpp
        presentation/ChartConfiguration.cpp presentation/ChartConfiguration.hpp
        presentation/ChartExporter.cpp presentation/ChartExporter.hpp
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
    QML_FILES qml/Meteogram.qml qml/ForecastPanel.qml qml/ForecastInspector.qml qml/ForecastHeader.qml qml/CompactForecast.qml qml/LocationEditor.qml qml/TileMap.qml
        qml/MeteoSettings.qml qml/MeteoAbout.qml qml/DisplaySettings.qml qml/WeatherMap.qml qml/EditorialView.qml qml/ThematicMaps.qml
        qml/MapTileFrame.qml qml/MapWheelHandler.qml qml/BufferedImage.qml
        qml/OriginalMeteogram.qml qml/MeteogramStyle.js qml/SeriesLegend.qml qml/SeriesSettings.qml
)
target_link_libraries(meteo_ui PUBLIC meteo_core Qt6::Qml PRIVATE Qt6::Quick Qt6::QuickControls2 KF6::Notifications)
target_include_directories(meteo_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/presentation")
target_compile_options(meteo_ui PRIVATE -Wall -Wextra -Wpedantic)

set(METEO_QML_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/qt6/qml/pl/meteo/desktop" CACHE PATH "QML module installation directory")
set_target_properties(meteo_ui meteo_uiplugin PROPERTIES INSTALL_RPATH "$ORIGIN")
install(TARGETS meteo_ui meteo_uiplugin LIBRARY DESTINATION "${METEO_QML_INSTALL_DIR}")
install(FILES
    "${QT_QML_OUTPUT_DIRECTORY}/pl/meteo/desktop/qmldir"
    "${QT_QML_OUTPUT_DIRECTORY}/pl/meteo/desktop/meteo_ui.qmltypes"
    DESTINATION "${METEO_QML_INSTALL_DIR}"
)
