add_library(meteo_core STATIC
    data/ForecastRepository.cpp data/ForecastRepository.hpp
    core/Forecast.cpp core/Forecast.hpp
    core/ForecastAggregation.cpp core/ForecastAggregation.hpp
    core/ForecastSampling.cpp core/ForecastSampling.hpp
    core/SolarCalendar.cpp core/SolarCalendar.hpp
    core/EditorialContent.cpp core/EditorialContent.hpp
    core/Localization.cpp core/Localization.hpp
    core/Locations.cpp core/Locations.hpp
    core/WebLink.cpp core/WebLink.hpp
    network/MeteoClient.cpp network/MeteoClient.hpp
    network/JsonRequest.cpp network/JsonRequest.hpp
    network/DiskCacheBudget.cpp network/DiskCacheBudget.hpp
    network/MeteoMapSource.cpp network/MeteoMapSource.hpp
    network/ThematicMapSource.cpp network/ThematicMapSource.hpp
)
target_include_directories(meteo_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(meteo_core PUBLIC Qt6::Core Qt6::Network KF6::I18n)

file(GLOB METEO_TRANSLATION_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/po/*/meteo-pl.po")
set(METEO_TRANSLATION_ROOT "${CMAKE_CURRENT_BINARY_DIR}/locale")
set(METEO_CATALOGS)
foreach(po_file IN LISTS METEO_TRANSLATION_FILES)
    get_filename_component(language_dir "${po_file}" DIRECTORY)
    get_filename_component(language "${language_dir}" NAME)
    set(catalog "${METEO_TRANSLATION_ROOT}/${language}/LC_MESSAGES/meteo-pl.mo")
    add_custom_command(OUTPUT "${catalog}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${METEO_TRANSLATION_ROOT}/${language}/LC_MESSAGES"
        COMMAND "${GETTEXT_MSGFMT_EXECUTABLE}" --check --check-format -o "${catalog}" "${po_file}"
        DEPENDS "${po_file}" VERBATIM)
    list(APPEND METEO_CATALOGS "${catalog}")
    install(FILES "${catalog}" DESTINATION "${CMAKE_INSTALL_LOCALEDIR}/${language}/LC_MESSAGES")
endforeach()
qt_add_resources(meteo_core "meteo_translations" PREFIX "/meteo/locale"
    BASE "${METEO_TRANSLATION_ROOT}" FILES ${METEO_CATALOGS})

foreach(component WidgetForecast Meteogram ForecastPanel ForecastInspector ForecastHeader WeatherSymbol PlacePicker SavedPlaceSelection CompactForecast LocationEditor TileMap MapTileFrame MapWheelHandler BufferedImage MeteoSettings ForecastSettings MeteoAbout DisplaySettings ReadingSettings ApplicationSettings SeriesLegend SeriesSettings WeatherMap EditorialView ThematicMaps OriginalMeteogram)
    set_source_files_properties(qml/${component}.qml PROPERTIES QT_RESOURCE_ALIAS ${component}.qml)
endforeach()
set_source_files_properties(qml/MeteogramStyle.js PROPERTIES QT_RESOURCE_ALIAS MeteogramStyle.js)
set_source_files_properties(qml/SettingsSection.qml PROPERTIES QT_RESOURCE_ALIAS SettingsSection.qml)
set_source_files_properties(qml/SeriesAppearanceDialog.qml PROPERTIES QT_RESOURCE_ALIAS SeriesAppearanceDialog.qml)
set_source_files_properties(qml/icons/sunrise.svg PROPERTIES QT_RESOURCE_ALIAS icons/sunrise.svg)
set_source_files_properties(qml/icons/sunset.svg PROPERTIES QT_RESOURCE_ALIAS icons/sunset.svg)
configure_file(qml/AppInfo.js.in "${CMAKE_CURRENT_BINARY_DIR}/AppInfo.js" @ONLY)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/AppInfo.js" PROPERTIES QT_RESOURCE_ALIAS AppInfo.js)
qt_add_qml_module(meteo_ui
    SHARED
    URI pl.meteo.desktop
    VERSION 1.0
    SOURCES presentation/WeatherModel.cpp presentation/WeatherModel.hpp
        presentation/I18n.hpp
        presentation/WebLinks.hpp
        presentation/ApplicationLauncher.cpp presentation/ApplicationLauncher.hpp
        presentation/LocationRules.hpp
        presentation/ChartAnalysis.cpp presentation/ChartAnalysis.hpp
        presentation/ChartDataBuilder.cpp presentation/ChartDataBuilder.hpp
        presentation/ForecastFields.cpp presentation/ForecastFields.hpp
        presentation/ForecastFormatting.cpp presentation/ForecastFormatting.hpp
        presentation/ChartConfiguration.cpp presentation/ChartConfiguration.hpp
        presentation/ChartExporter.cpp presentation/ChartExporter.hpp
        presentation/ReadingsExporter.cpp presentation/ReadingsExporter.hpp
        presentation/ExportNotifications.cpp presentation/ExportNotifications.hpp
        presentation/GpuPlot.cpp presentation/GpuPlot.hpp
        presentation/ResponsiveImages.cpp presentation/ResponsiveImages.hpp
        presentation/ImagePreloader.cpp presentation/ImagePreloader.hpp
        presentation/CacheStorage.cpp presentation/CacheStorage.hpp
        presentation/PlaceSearchModel.cpp presentation/PlaceSearchModel.hpp
        presentation/ReverseGeocoder.cpp presentation/ReverseGeocoder.hpp
        presentation/UserPreferences.cpp presentation/UserPreferences.hpp
        presentation/ProfileFileCodec.cpp presentation/ProfileFileCodec.hpp
        presentation/JsonSchemaValidator.cpp presentation/JsonSchemaValidator.hpp
        presentation/PreferencesStore.cpp presentation/PreferencesStore.hpp
        presentation/SavedPlaceController.cpp presentation/SavedPlaceController.hpp
        presentation/WeatherLayersModel.cpp presentation/WeatherLayersModel.hpp
        presentation/MapLegendModel.cpp presentation/MapLegendModel.hpp
        presentation/ThematicMapsModel.cpp presentation/ThematicMapsModel.hpp
        presentation/EditorialModel.cpp presentation/EditorialModel.hpp
        presentation/MapImages.cpp presentation/MapImages.hpp
        presentation/LegacyMeteogramModel.cpp presentation/LegacyMeteogramModel.hpp
        network/ImageCache.cpp network/ImageCache.hpp
    RESOURCES qml/icons/sunrise.svg qml/icons/sunset.svg
    QML_FILES qml/WidgetForecast.qml "${CMAKE_CURRENT_BINARY_DIR}/AppInfo.js" qml/Meteogram.qml qml/ForecastPanel.qml qml/ForecastInspector.qml qml/ForecastHeader.qml qml/WeatherSymbol.qml qml/PlacePicker.qml qml/SavedPlaceSelection.qml qml/CompactForecast.qml qml/LocationEditor.qml qml/TileMap.qml
        qml/MeteoSettings.qml qml/SettingsSection.qml qml/ForecastSettings.qml qml/MeteoAbout.qml qml/DisplaySettings.qml qml/ReadingSettings.qml qml/ApplicationSettings.qml qml/WeatherMap.qml qml/EditorialView.qml qml/ThematicMaps.qml
        qml/MapTileFrame.qml qml/MapWheelHandler.qml qml/BufferedImage.qml
        qml/OriginalMeteogram.qml qml/MeteogramStyle.js qml/SeriesLegend.qml qml/SeriesSettings.qml qml/SeriesAppearanceDialog.qml
)
foreach(component PlaceEditorDialog PlacesView PlacesDialog MobilePlacesPage ChartAxesEditor CalendarDialog DateTimeField TimeRangeDialog)
    set_source_files_properties(qml/${component}.qml PROPERTIES QT_RESOURCE_ALIAS ${component}.qml)
    qt_target_qml_sources(meteo_ui QML_FILES qml/${component}.qml)
endforeach()
set_source_files_properties(qml/WidgetChartSelection.qml PROPERTIES QT_RESOURCE_ALIAS WidgetChartSelection.qml)
qt_target_qml_sources(meteo_ui QML_FILES qml/WidgetChartSelection.qml)
set_source_files_properties(qml/ChartGestures.qml PROPERTIES QT_RESOURCE_ALIAS ChartGestures.qml)
qt_target_qml_sources(meteo_ui QML_FILES qml/ChartGestures.qml)
foreach(component MobilePageHeader MobileLocationPage MobileSettingsPage MobileForecastPage MobileSheet MobileHourlyForecast)
    set_source_files_properties(qml/${component}.qml PROPERTIES QT_RESOURCE_ALIAS ${component}.qml)
    qt_target_qml_sources(meteo_ui QML_FILES qml/${component}.qml)
endforeach()
set_source_files_properties(qml/ChartSaveDialog.qml PROPERTIES QT_RESOURCE_ALIAS ChartSaveDialog.qml)
qt_target_qml_sources(meteo_ui QML_FILES qml/ChartSaveDialog.qml)
set_source_files_properties(qml/PreferenceWriteNotice.qml PROPERTIES QT_RESOURCE_ALIAS PreferenceWriteNotice.qml)
qt_target_qml_sources(meteo_ui QML_FILES qml/PreferenceWriteNotice.qml)
set_source_files_properties(qml/HourlyForecastTable.qml PROPERTIES QT_RESOURCE_ALIAS HourlyForecastTable.qml)
qt_target_qml_sources(meteo_ui QML_FILES qml/HourlyForecastTable.qml)
set_source_files_properties(qml/ProfileUpdateNotice.qml PROPERTIES QT_RESOURCE_ALIAS ProfileUpdateNotice.qml)
qt_target_qml_sources(meteo_ui QML_FILES qml/ProfileUpdateNotice.qml)
target_link_libraries(meteo_ui PUBLIC meteo_core Qt6::Qml PRIVATE Qt6::Quick Qt6::QuickControls2 KF6::Notifications)
target_include_directories(meteo_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/presentation")
target_compile_definitions(meteo_ui PRIVATE METEO_UI_BUILD)
qt_add_resources(meteo_ui "license" PREFIX "/meteo" BASE "${PROJECT_SOURCE_DIR}"
    FILES "${PROJECT_SOURCE_DIR}/LICENSE")
file(GLOB METEO_PROFILE_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/resources/profiles/*.json")
qt_add_resources(meteo_ui "chart_profiles" PREFIX "/meteo/profiles"
    BASE "${PROJECT_SOURCE_DIR}/resources/profiles" FILES ${METEO_PROFILE_FILES})
install(FILES ${METEO_PROFILE_FILES} DESTINATION "${CMAKE_INSTALL_DATADIR}/meteo-pl/profiles")
file(GLOB METEO_CHART_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/resources/charts/*.json" "${PROJECT_SOURCE_DIR}/resources/schemas/*.json")
qt_add_resources(meteo_ui "chart_defaults" PREFIX "/meteo"
    BASE "${PROJECT_SOURCE_DIR}/resources" FILES ${METEO_CHART_FILES})
install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/charts" "${PROJECT_SOURCE_DIR}/resources/schemas"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/meteo-pl")
include(FetchContent)
FetchContent_Declare(meteo_valijson
    URL https://codeload.github.com/tristanpenman/valijson/tar.gz/5eac4b66f20b2c189ae40281c29e71cf39a958ec
    URL_HASH SHA256=8b68741da04708c806d07a5a5fd181d3fb801aa161d7f65f45d7a3cc1b0c98ee
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR meteo-headers-only)
FetchContent_MakeAvailable(meteo_valijson)
target_include_directories(meteo_ui SYSTEM PRIVATE "${meteo_valijson_SOURCE_DIR}/include")
target_compile_definitions(meteo_ui PRIVATE VALIJSON_USE_EXCEPTIONS=1)
install(FILES "${meteo_valijson_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/meteo-pl/valijson")
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

set_source_files_properties(qml/DailyForecastView.qml PROPERTIES QT_RESOURCE_ALIAS DailyForecastView.qml)
qt_target_qml_sources(meteo_ui QML_FILES qml/DailyForecastView.qml)
set_source_files_properties(qml/MapLegend.qml PROPERTIES QT_RESOURCE_ALIAS MapLegend.qml)
qt_target_qml_sources(meteo_ui QML_FILES qml/MapLegend.qml)
