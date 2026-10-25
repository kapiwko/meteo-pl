qt_add_executable(test_forecast test_forecast.cpp)
target_link_libraries(test_forecast PRIVATE meteo_core Qt6::Test)
add_test(NAME forecast COMMAND test_forecast)

qt_add_executable(test_client test_client.cpp)
target_link_libraries(test_client PRIVATE meteo_core Qt6::Test)
add_test(NAME client COMMAND test_client)

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
add_test(NAME presentation COMMAND test_presentation)
set_tests_properties(presentation PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}")

qt_add_executable(test_maps test_maps.cpp)
target_link_libraries(test_maps PRIVATE meteo_ui Qt6::Test Qt6::Quick Qt6::QuickControls2)
add_test(NAME maps COMMAND test_maps)
set_tests_properties(maps PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}")

if(METEO_BUILD_APP)
    add_test(NAME app_smoke COMMAND meteo-pl --smoke-test)
    set_tests_properties(app_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY};XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/config"
        TIMEOUT 15
        FAIL_REGULAR_EXPRESSION "TypeError;ReferenceError;Binding loop;failed to load component"
    )
endif()

if(METEO_BUILD_PLASMOID)
    find_program(PLASMAWINDOWED_EXECUTABLE plasmawindowed)
    if(PLASMAWINDOWED_EXECUTABLE)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        add_test(NAME plasma_smoke COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/plasma-smoke.py" --program "${PLASMAWINDOWED_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/integrations/plasma/package")
        set_tests_properties(plasma_smoke PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY};XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/plasma-config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/plasma-cache"
            TIMEOUT 15
            FAIL_REGULAR_EXPRESSION "TypeError;ReferenceError;Binding loop;Error loading QML file;failed to load component"
        )
    endif()
endif()

qt_add_executable(test_renderers test_renderers.cpp)
target_link_libraries(test_renderers PRIVATE meteo_ui Qt6::Test Qt6::Quick Qt6::QuickControls2)
add_test(NAME renderer_parity COMMAND test_renderers)
set_tests_properties(renderer_parity PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=rhi;QSG_RHI_BACKEND=opengl;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
    TIMEOUT 120
    LABELS "gpu;visual"
)

find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
add_test(NAME release_archives COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_release.py")
set_tests_properties(release_archives PROPERTIES TIMEOUT 15)
add_test(NAME portable_packaging COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_portable.py")
set_tests_properties(portable_packaging PROPERTIES TIMEOUT 15)
