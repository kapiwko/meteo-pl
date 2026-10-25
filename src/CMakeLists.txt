add_library(meteo_core STATIC
    core/Forecast.cpp
    core/Forecast.hpp
    network/MeteoClient.cpp
    network/MeteoClient.hpp
)
target_include_directories(meteo_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(meteo_core PUBLIC Qt6::Core Qt6::Network)
target_compile_options(meteo_core PRIVATE -Wall -Wextra -Wpedantic)
