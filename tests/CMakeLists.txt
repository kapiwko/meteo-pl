qt_add_executable(test_forecast test_forecast.cpp)
target_link_libraries(test_forecast PRIVATE meteo_core Qt6::Test)
add_test(NAME forecast COMMAND test_forecast)

qt_add_executable(test_client test_client.cpp)
target_link_libraries(test_client PRIVATE meteo_core Qt6::Test)
add_test(NAME client COMMAND test_client)

qt_add_executable(test_services test_services.cpp)
target_link_libraries(test_services PRIVATE meteo_ui Qt6::Test)
add_test(NAME services COMMAND test_services)

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
