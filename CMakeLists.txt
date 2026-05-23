cmake_minimum_required(VERSION 3.20)
project(StockMarketTracker VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

# ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.6
)
FetchContent_MakeAvailable(imgui)

find_package(OpenGL REQUIRED)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw OpenGL::GL)

# ImPlot
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG v0.17
)
FetchContent_MakeAvailable(implot)

add_library(implot_lib STATIC
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(implot_lib PUBLIC ${implot_SOURCE_DIR})
target_link_libraries(implot_lib PUBLIC imgui_lib)

# nlohmann_json
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Catch2
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

# Use pre-built MinGW curl at C:/curl-mingw if present (no system install)
if(WIN32 AND EXISTS "C:/curl-mingw/include/curl/curl.h")
    set(CURL_INCLUDE_DIR "C:/curl-mingw/include" CACHE PATH "" FORCE)
    set(CURL_LIBRARY    "C:/curl-mingw/lib/libcurl.dll.a" CACHE FILEPATH "" FORCE)
endif()
find_package(CURL REQUIRED)

add_executable(StockTracker
    src/main.cpp
    src/yahoo_provider.cpp
    src/stock_tracker.cpp
)
target_include_directories(StockTracker PRIVATE include)
target_link_libraries(StockTracker PRIVATE
    imgui_lib
    implot_lib
    glfw
    OpenGL::GL
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(StockTracker PRIVATE opengl32)
    if(EXISTS "C:/curl-mingw/bin/libcurl-x64.dll")
        add_custom_command(TARGET StockTracker POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "C:/curl-mingw/bin/libcurl-x64.dll"
                $<TARGET_FILE_DIR:StockTracker>
        )
    endif()
endif()

enable_testing()
add_subdirectory(tests)
