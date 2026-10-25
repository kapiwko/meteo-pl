qt_add_executable(test_forecast test_forecast.cpp)
target_link_libraries(test_forecast PRIVATE meteo_core Qt6::Test)
add_test(NAME forecast COMMAND test_forecast)

qt_add_executable(test_client test_client.cpp)
target_link_libraries(test_client PRIVATE meteo_core Qt6::Test)
add_test(NAME client COMMAND test_client)

