get_filename_component(KISAK_SCRIPT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_library(kisakcod_script_support STATIC
    "${KISAK_SCRIPT_ROOT}/src/universal/com_hunkuser.cpp"
    "${KISAK_SCRIPT_ROOT}/src/script/scr_memorytree.cpp"
    "${KISAK_SCRIPT_ROOT}/src/script/scr_stack_buffer.cpp"
    "${KISAK_SCRIPT_ROOT}/src/script/scr_value_io.cpp"
    "${KISAK_SCRIPT_ROOT}/src/script/scr_bytecode.cpp"
    "${KISAK_SCRIPT_ROOT}/src/script/scr_vector.cpp"
    "${KISAK_SCRIPT_ROOT}/src/script/scr_variable_state.cpp"
    "${KISAK_SCRIPT_ROOT}/src/script/scr_stringlist.cpp"
    "${KISAK_SCRIPT_ROOT}/src/script/scr_compile_state.cpp"
    "${KISAK_SCRIPT_ROOT}/src/script/scr_parsetree.cpp"
    "${KISAK_SCRIPT_ROOT}/src/script/scr_yacc2.cpp"
)
target_compile_features(kisakcod_script_support PUBLIC cxx_std_20)
target_compile_definitions(kisakcod_script_support PUBLIC KISAK_SP)
target_compile_options(kisakcod_script_support PRIVATE -fms-extensions)
target_link_libraries(kisakcod_script_support PUBLIC kisakcod_apple_platform kisakcod_save_support)
if(BUILD_TESTING AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(kisakcod_script_support_tests tests/ScriptSupportTests.cpp tests/TestEngineServices.cpp)
    target_link_libraries(kisakcod_script_support_tests PRIVATE kisakcod_script_support)
    add_test(NAME kisakcod_script_support_tests COMMAND kisakcod_script_support_tests)
    set_tests_properties(kisakcod_script_support_tests PROPERTIES TIMEOUT 30)
    add_executable(kisakcod_script_frontend_tests tests/ScriptFrontendTests.cpp tests/TestEngineServices.cpp)
    target_link_libraries(kisakcod_script_frontend_tests PRIVATE kisakcod_script_support)
    add_test(NAME kisakcod_script_frontend_tests COMMAND kisakcod_script_frontend_tests)
    set_tests_properties(kisakcod_script_frontend_tests PROPERTIES TIMEOUT 30)
    add_executable(kisakcod_script_stack_tests tests/ScriptStackTests.cpp tests/TestEngineServices.cpp)
    target_link_libraries(kisakcod_script_stack_tests PRIVATE kisakcod_script_support)
    add_test(NAME kisakcod_script_stack_tests COMMAND kisakcod_script_stack_tests)
    set_tests_properties(kisakcod_script_stack_tests PROPERTIES TIMEOUT 30)
    add_executable(kisakcod_script_save_tests tests/ScriptSaveTests.cpp tests/TestEngineServices.cpp)
    target_link_libraries(kisakcod_script_save_tests PRIVATE kisakcod_script_support)
    add_test(NAME kisakcod_script_save_tests COMMAND kisakcod_script_save_tests)
    set_tests_properties(kisakcod_script_save_tests PROPERTIES TIMEOUT 30)
    add_executable(kisakcod_script_bytecode_tests tests/ScriptBytecodeTests.cpp tests/TestEngineServices.cpp)
    target_link_libraries(kisakcod_script_bytecode_tests PRIVATE kisakcod_script_support)
    add_test(NAME kisakcod_script_bytecode_tests COMMAND kisakcod_script_bytecode_tests)
    set_tests_properties(kisakcod_script_bytecode_tests PROPERTIES TIMEOUT 30)
endif()

add_library(kisakcod_script_variables STATIC "${KISAK_SCRIPT_ROOT}/src/script/scr_variable.cpp")
target_compile_options(kisakcod_script_variables PRIVATE -fms-extensions)
target_link_libraries(kisakcod_script_variables PUBLIC kisakcod_script_support)
if(BUILD_TESTING AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(kisakcod_script_variable_tests tests/VariableTests.cpp tests/VariableTestServices.cpp tests/TestEngineServices.cpp)
    target_link_libraries(kisakcod_script_variable_tests PRIVATE kisakcod_script_variables)
    target_link_options(kisakcod_script_variable_tests PRIVATE -Wl,-dead_strip)
    add_test(NAME kisakcod_script_variable_tests COMMAND kisakcod_script_variable_tests)
    set_tests_properties(kisakcod_script_variable_tests PROPERTIES TIMEOUT 30)
endif()
