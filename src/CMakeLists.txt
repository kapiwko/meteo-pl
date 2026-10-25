add_library(meteo_core STATIC
    core/Forecast.cpp
    core/Forecast.hpp
    core/Locations.cpp
    core/Locations.hpp
    network/MeteoClient.cpp
    network/MeteoClient.hpp
    network/JsonRequest.cpp
    network/JsonRequest.hpp
)
target_include_directories(meteo_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(meteo_core PUBLIC Qt6::Core Qt6::Network)
target_compile_options(meteo_core PRIVATE -Wall -Wextra -Wpedantic)

set_source_files_properties(qml/BufferedImage.qml PROPERTIES QT_RESOURCE_ALIAS BufferedImage.qml)
set_source_files_properties(qml/DisplaySettings.qml PROPERTIES QT_RESOURCE_ALIAS DisplaySettings.qml)
set_source_files_properties(qml/EditorialView.qml PROPERTIES QT_RESOURCE_ALIAS EditorialView.qml)
set_source_files_properties(qml/ForecastPanel.qml PROPERTIES QT_RESOURCE_ALIAS ForecastPanel.qml)
set_source_files_properties(qml/LocationEditor.qml PROPERTIES QT_RESOURCE_ALIAS LocationEditor.qml)
set_source_files_properties(qml/MapTileFrame.qml PROPERTIES QT_RESOURCE_ALIAS MapTileFrame.qml)
set_source_files_properties(qml/MapWheelHandler.qml PROPERTIES QT_RESOURCE_ALIAS MapWheelHandler.qml)
set_source_files_properties(qml/Meteogram.qml PROPERTIES QT_RESOURCE_ALIAS Meteogram.qml)
set_source_files_properties(qml/OriginalMeteogram.qml PROPERTIES QT_RESOURCE_ALIAS OriginalMeteogram.qml)
set_source_files_properties(qml/ThematicMaps.qml PROPERTIES QT_RESOURCE_ALIAS ThematicMaps.qml)
set_source_files_properties(qml/TileMap.qml PROPERTIES QT_RESOURCE_ALIAS TileMap.qml)
set_source_files_properties(qml/WeatherMap.qml PROPERTIES QT_RESOURCE_ALIAS WeatherMap.qml)
qt_add_qml_module(meteo_ui
    SHARED
    URI pl.meteo.desktop
    VERSION 1.0
    NO_PLUGIN_OPTIONAL
    SOURCES
        presentation/EditorialModel.cpp
        presentation/EditorialModel.hpp
        presentation/ForecastFields.cpp
        presentation/ForecastFields.hpp
        presentation/GpuPlot.cpp
        presentation/GpuPlot.hpp
        presentation/LegacyMeteogramModel.cpp
        presentation/LegacyMeteogramModel.hpp
        presentation/MapImages.cpp
        presentation/MapImages.hpp
        presentation/PlaceSearchModel.cpp
        presentation/PlaceSearchModel.hpp
        presentation/UserPreferences.cpp
        presentation/UserPreferences.hpp
        presentation/WeatherLayersModel.cpp
        presentation/WeatherLayersModel.hpp
        presentation/WeatherModel.cpp
        presentation/WeatherModel.hpp
        network/ImageCache.cpp
        network/ImageCache.hpp
    QML_FILES
        qml/BufferedImage.qml
        qml/DisplaySettings.qml
        qml/EditorialView.qml
        qml/ForecastPanel.qml
        qml/LocationEditor.qml
        qml/MapTileFrame.qml
        qml/MapWheelHandler.qml
        qml/Meteogram.qml
        qml/OriginalMeteogram.qml
        qml/ThematicMaps.qml
        qml/TileMap.qml
        qml/WeatherMap.qml
)
target_link_libraries(meteo_ui PUBLIC meteo_core Qt6::Qml PRIVATE Qt6::Quick Qt6::QuickControls2)
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
