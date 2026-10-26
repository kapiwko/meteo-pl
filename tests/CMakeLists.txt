qt_add_executable(test_forecast test_forecast.cpp)
target_link_libraries(test_forecast PRIVATE meteo_core Qt6::Test)
add_test(NAME forecast COMMAND test_forecast)

qt_add_executable(test_solar test_solar.cpp)
target_link_libraries(test_solar PRIVATE meteo_core Qt6::Test)
add_test(NAME solar COMMAND test_solar)

qt_add_executable(test_client test_client.cpp)
target_link_libraries(test_client PRIVATE meteo_core Qt6::Test)
add_test(NAME client COMMAND test_client)

qt_add_executable(test_repository test_repository.cpp)
target_link_libraries(test_repository PRIVATE meteo_core Qt6::Test)
add_test(NAME repository COMMAND test_repository)

qt_add_executable(test_disk_cache test_disk_cache.cpp)
target_link_libraries(test_disk_cache PRIVATE meteo_core Qt6::Test)
add_test(NAME disk_cache COMMAND test_disk_cache)

qt_add_executable(test_chart_analysis test_chart_analysis.cpp)
target_link_libraries(test_chart_analysis PRIVATE meteo_ui Qt6::Test)
add_test(NAME chart_analysis COMMAND test_chart_analysis)
set_tests_properties(chart_analysis PROPERTIES TIMEOUT 10)

qt_add_executable(test_i18n test_i18n.cpp)
target_link_libraries(test_i18n PRIVATE meteo_ui Qt6::Test Qt6::Quick)
foreach(language en_US pl_PL)
    add_test(NAME translation_${language} COMMAND test_i18n)
    set_tests_properties(translation_${language} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LC_ALL=en_US.UTF-8;LANGUAGE=${language};METEO_TEST_LANGUAGE=${language};QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}")
endforeach()

qt_add_executable(test_services test_services.cpp)
target_link_libraries(test_services PRIVATE meteo_ui Qt6::Test)
add_test(NAME services COMMAND test_services)

qt_add_executable(test_profile_codec test_profile_codec.cpp)
target_link_libraries(test_profile_codec PRIVATE meteo_ui Qt6::Test)
add_test(NAME profile_codec COMMAND test_profile_codec)

qt_add_executable(test_places test_places.cpp)
target_link_libraries(test_places PRIVATE meteo_ui Qt6::Test)
add_test(NAME places COMMAND test_places)

qt_add_executable(test_preferences test_preferences.cpp)
target_link_libraries(test_preferences PRIVATE meteo_ui Qt6::Test)
add_test(NAME preferences COMMAND test_preferences)
set_tests_properties(preferences PROPERTIES ENVIRONMENT "QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}")

qt_add_executable(test_image_cache test_image_cache.cpp)
target_link_libraries(test_image_cache PRIVATE meteo_ui Qt6::Test Qt6::Gui)
add_test(NAME image_cache COMMAND test_image_cache)
set_tests_properties(image_cache PROPERTIES TIMEOUT 20)

qt_add_executable(test_presentation test_presentation.cpp)
target_link_libraries(test_presentation PRIVATE meteo_ui Qt6::Test Qt6::Quick Qt6::QuickControls2)
target_compile_definitions(test_presentation PRIVATE METEO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
if(METEO_BUILD_PLASMOID)
    target_compile_definitions(test_presentation PRIVATE METEO_TEST_PLASMA)
endif()
add_test(NAME presentation COMMAND test_presentation)
set_tests_properties(presentation PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}")
add_test(NAME mobile_material_ui COMMAND test_presentation mobileNavigationAndSettingsUsePages)
set_tests_properties(mobile_material_ui PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;METEO_TEST_STYLE=Material;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
    TIMEOUT 60)
add_test(NAME mobile_material_dark_ui COMMAND test_presentation mobileNavigationAndSettingsUsePages)
set_tests_properties(mobile_material_dark_ui PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;METEO_TEST_STYLE=Material;QT_QUICK_CONTROLS_MATERIAL_THEME=Dark;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
    TIMEOUT 60)

qt_add_executable(test_maps test_maps.cpp)
target_link_libraries(test_maps PRIVATE meteo_ui Qt6::Test Qt6::Quick Qt6::QuickControls2)
add_test(NAME maps COMMAND test_maps)
set_tests_properties(maps PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}")

if(METEO_BUILD_APP)
    add_test(NAME app_smoke COMMAND meteo-pl --smoke-test)
    set_tests_properties(app_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY};XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/config"
        TIMEOUT 15
        FAIL_REGULAR_EXPRESSION "TypeError;ReferenceError;Binding loop;failed to load component"
    )
endif()

if(METEO_BUILD_PLASMOID)
    find_program(PLASMAWINDOWED_EXECUTABLE plasmawindowed)
    if(PLASMAWINDOWED_EXECUTABLE)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        qt_add_qml_module(meteo_test_fixtures SHARED URI pl.meteo.testfixtures SOURCES plasma/FixtureWeatherModel.hpp)
        target_link_libraries(meteo_test_fixtures PRIVATE meteo_ui Qt6::Quick Qt6::Test)
        target_include_directories(meteo_test_fixtures PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/plasma")
        add_test(NAME plasma_interactions COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/plasma-interactions.py"
            --program "${PLASMAWINDOWED_EXECUTABLE}" --qml-path "${QT_QML_OUTPUT_DIRECTORY}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/plasma-artifacts")
        set_tests_properties(plasma_interactions PROPERTIES TIMEOUT 100 LABELS "plasma;integration")
        add_test(NAME plasma_smoke COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/plasma-smoke.py" --program "${PLASMAWINDOWED_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/integrations/plasma/package")
        set_tests_properties(plasma_smoke PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY};XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/plasma-config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/plasma-cache"
            TIMEOUT 15
            FAIL_REGULAR_EXPRESSION "TypeError;ReferenceError;Binding loop;Error loading QML file;failed to load component"
        )
    endif()
endif()

qt_add_executable(test_renderers test_renderers.cpp)
target_link_libraries(test_renderers PRIVATE meteo_ui Qt6::Test Qt6::Quick Qt6::QuickControls2)
# Qt 6.8's offscreen/GLX readback can corrupt the window header. Use a real
# virtual X11 surface for RHI image checks when available; keep RHI mandatory.
set(METEO_GPU_QPA offscreen)
set(METEO_GPU_TEST_WRAPPER)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(METEO_XVFB_RUN xvfb-run)
    if(METEO_XVFB_RUN)
        set(METEO_GPU_QPA xcb)
        set(METEO_GPU_TEST_WRAPPER "${METEO_XVFB_RUN}" --auto-servernum)
    endif()
endif()
add_test(NAME renderer_parity COMMAND ${METEO_GPU_TEST_WRAPPER} $<TARGET_FILE:test_renderers>)
set_tests_properties(renderer_parity PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=${METEO_GPU_QPA};QT_QUICK_BACKEND=rhi;QSG_RHI_BACKEND=opengl;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
    TIMEOUT 120
    LABELS "gpu;visual"
)

add_test(NAME gpu_geometry COMMAND ${METEO_GPU_TEST_WRAPPER} $<TARGET_FILE:test_presentation> nativeGpuRendererDrawsClippedGeometry)
set_tests_properties(gpu_geometry PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=${METEO_GPU_QPA};QT_QUICK_BACKEND=rhi;QSG_RHI_BACKEND=opengl;METEO_REQUIRE_GPU=1;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
    TIMEOUT 30
    LABELS "gpu;geometry"
)

find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
add_test(NAME release_archives COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_release.py")
set_tests_properties(release_archives PROPERTIES TIMEOUT 15)
add_test(NAME portable_packaging COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_portable.py")
set_tests_properties(portable_packaging PROPERTIES TIMEOUT 15)
add_test(NAME bundle_runtime COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_bundle_runtime.py")
set_tests_properties(bundle_runtime PROPERTIES TIMEOUT 15)

# Qt may otherwise send diagnostics only to journald when stdout is captured.
get_property(meteo_tests DIRECTORY PROPERTY TESTS)
set_tests_properties(${meteo_tests} PROPERTIES ENVIRONMENT_MODIFICATION "QT_FORCE_STDERR_LOGGING=set:1")
