cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(KisakCODPortableCore LANGUAGES CXX)
    include(CTest)
endif()

get_filename_component(KISAK_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

# Read the original multiplayer frequency table directly. This avoids a second
# hand-maintained copy while keeping the Windows multiplayer module out of iOS.
set(KISAK_HUFFMAN_TABLE_SOURCE "${KISAK_CORE_ROOT}/src/qcommon/msg_mp.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${KISAK_HUFFMAN_TABLE_SOURCE}")
file(READ "${KISAK_HUFFMAN_TABLE_SOURCE}" KISAK_MSG_SOURCE)
string(REGEX MATCH "const int msg_hData\\[256\\][ \r\n]*=[ \r\n]*\\{([^}]+)\\}" KISAK_TABLE_MATCH "${KISAK_MSG_SOURCE}")
if(NOT KISAK_TABLE_MATCH)
    message(FATAL_ERROR "Cannot locate the upstream multiplayer Huffman table")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/kisak_huffman_data.h"
    "#pragma once\ninline constexpr int kisak_huffman_data[] = {${CMAKE_MATCH_1}};\nstatic_assert(sizeof(kisak_huffman_data) / sizeof(int) == 256);\n")

# Upstream modules verified to build and link on ARM64 without the Windows
# platform layer. Modules that still reach Sys_*/threads.h (q_shared.cpp and
# therefore surfaceflags.cpp) stay out until those services are ported.
add_library(kisakcod_core STATIC
    "${KISAK_CORE_ROOT}/src/qcommon/md4.cpp"
    "${KISAK_CORE_ROOT}/src/qcommon/huffman.cpp"
    "${KISAK_CORE_ROOT}/src/universal/com_math.cpp"
    "${KISAK_CORE_ROOT}/src/universal/com_math_anglevectors.cpp"
    "${KISAK_CORE_ROOT}/src/universal/base64.cpp"
    "${KISAK_CORE_ROOT}/src/universal/pool_allocator.cpp"
    "${KISAK_CORE_ROOT}/src/qcommon/com_pack.cpp"
    kisak_core.cpp
    core_assert.cpp
    core_services.cpp
)
target_compile_features(kisakcod_core PUBLIC cxx_std_17)
# src/ and deps/ are public so the app can call the reused engine modules
# through their original headers instead of restating their declarations.
target_include_directories(kisakcod_core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
           "${KISAK_CORE_ROOT}/src" "${KISAK_CORE_ROOT}/deps"
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
)
set_target_properties(kisakcod_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

option(KISAK_CORE_SANITIZERS "Enable address/undefined sanitizers for host core tests" OFF)
if(KISAK_CORE_SANITIZERS AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_options(kisakcod_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(kisakcod_core PUBLIC -fsanitize=address,undefined)
endif()

if(BUILD_TESTING AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../tests" "${CMAKE_CURRENT_BINARY_DIR}/tests")
endif()
