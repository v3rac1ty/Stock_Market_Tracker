add_executable(tests test_main.cpp
    ../src/yahoo_provider.cpp
    ../src/stock_tracker.cpp
)
target_include_directories(tests PRIVATE ../include)
target_link_libraries(tests PRIVATE
    Catch2::Catch2WithMain
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

if(WIN32 AND EXISTS "C:/curl-mingw/bin/libcurl-x64.dll")
    add_custom_command(TARGET tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/curl-mingw/bin/libcurl-x64.dll"
            $<TARGET_FILE_DIR:tests>
    )
endif()

include(CTest)
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)
catch_discover_tests(tests DISCOVERY_MODE PRE_TEST)
