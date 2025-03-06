cmake_minimum_required(VERSION 3.10)
project(StockMarketTracker VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(CURL REQUIRED)
find_package(nlohmann_json 3.2.0 QUIET)

# If nlohmann_json is not found via find_package, use FetchContent as a fallback
if(NOT nlohmann_json_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2  # Specify a version/tag
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/stock_data.cpp
    src/stock_tracker.cpp
    src/yahoo_provider.cpp
)

# Define the executable
add_executable(stock_market_tracker ${SOURCES})

# Include directories
target_include_directories(stock_market_tracker 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(stock_market_tracker
    PRIVATE
    ${CURL_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Compiler warnings
if(MSVC)
    target_compile_options(stock_market_tracker PRIVATE /W4)
else()
    target_compile_options(stock_market_tracker PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install configuration
install(TARGETS stock_market_tracker DESTINATION bin)

# Add README.md to show in IDE
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT stock_market_tracker)

# Set build configurations
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

# Output directory configuration
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# Debug configurations
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        target_compile_options(stock_market_tracker PRIVATE /Od /Zi)
    else()
        target_compile_options(stock_market_tracker PRIVATE -O0 -g)
    endif()
endif()

# Release configurations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(stock_market_tracker PRIVATE /O2)
    else()
        target_compile_options(stock_market_tracker PRIVATE -O3)
    endif()
endif()

# Print configuration info
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "CURL Found: ${CURL_FOUND}")
message(STATUS "nlohmann_json Found: ${nlohmann_json_FOUND}")
