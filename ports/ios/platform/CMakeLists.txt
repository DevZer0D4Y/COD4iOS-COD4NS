# Genuine Apple implementations of a subset of the engine Sys_* services.
# This is not yet a complete platform backend or a campaign executable.
add_library(kisakcod_apple_platform STATIC apple_events.cpp apple_timing.cpp apple_critical_sections.cpp apple_virtual_memory.cpp)
target_compile_features(kisakcod_apple_platform PUBLIC cxx_std_17)
target_compile_definitions(kisakcod_apple_platform PRIVATE KISAK_SP)
target_include_directories(kisakcod_apple_platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(kisakcod_apple_platform PUBLIC kisakcod_core)
if(BUILD_TESTING AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(kisakcod_platform_tests tests/PlatformTests.cpp)
    target_compile_definitions(kisakcod_platform_tests PRIVATE KISAK_SP)
    target_link_libraries(kisakcod_platform_tests PRIVATE kisakcod_apple_platform)
    add_test(NAME kisakcod_platform_tests COMMAND kisakcod_platform_tests)
    set_tests_properties(kisakcod_platform_tests PROPERTIES TIMEOUT 15)
endif()
