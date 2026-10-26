# Android has no system icon theme. Kirigami loads this small, installed
# fallback theme; KIconTheme::initTheme() deliberately does nothing there.
include(FetchContent)
include(KDEInstallDirs6)
if(NOT BREEZEICONS_DIR)
    FetchContent_Declare(meteo_breeze_icons
        URL https://download.kde.org/stable/frameworks/6.31/breeze-icons-6.31.0.tar.xz
        URL_HASH SHA256=82ab1c13f782cb37176e7c13c5fb2c77536a65f24b85d09127f3575f917fb56d
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR meteo-icons-only)
    FetchContent_MakeAvailable(meteo_breeze_icons)
    set(BREEZEICONS_DIR "${meteo_breeze_icons_SOURCE_DIR}")
endif()
kirigami_package_breeze_icons(ICONS
    arrow-down arrow-right arrow-up chronometer color-picker configure
    dialog-close dialog-information-symbolic dialog-messages
    document-export document-import document-open document-save-as
    edit-clear edit-delete edit-duplicate find-location folder-open globe
    go-down go-next go-previous go-up help-about image-png internet-web-browser
    list-add list-remove map-globe mark-location media-playback-pause
    media-playback-start office-chart-line pin text-csv view-calendar
    view-list-text view-refresh view-statistics weather-few-clouds
    weather-none-available window-pin zoom-fit-best zoom-in zoom-out)
install(FILES "${BREEZEICONS_DIR}/COPYING-ICONS" "${BREEZEICONS_DIR}/COPYING.LIB"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/meteo-pl/breeze-icons")
