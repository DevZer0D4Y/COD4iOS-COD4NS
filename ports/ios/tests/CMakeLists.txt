add_executable(kisakcod_core_tests core_tests.cpp)
target_link_libraries(kisakcod_core_tests PRIVATE kisakcod_core)
target_include_directories(kisakcod_core_tests PRIVATE
    "${KISAK_CORE_ROOT}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/.."
)
add_test(NAME kisakcod_core_tests COMMAND kisakcod_core_tests)
add_executable(kisakcod_pool_tests pool_tests.cpp)
target_link_libraries(kisakcod_pool_tests PRIVATE kisakcod_core)
add_test(NAME kisakcod_pool_tests COMMAND kisakcod_pool_tests)
