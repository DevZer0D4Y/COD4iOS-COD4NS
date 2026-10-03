# 64-bit fastfile loading for KisakCOD, built from OpenAssetTools (GPLv3):
# its zone stream runtime (oat/, trimmed to IW3) plus loaders and markers
# generated from the IW3 zone definitions (generated/). See ../PORTING_STATUS.md.

file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/oat_sources.txt" KISAK_OAT_SOURCES_RELATIVE)
set(KISAK_OAT_SOURCES "")
foreach(source IN LISTS KISAK_OAT_SOURCES_RELATIVE)
    list(APPEND KISAK_OAT_SOURCES "${CMAKE_CURRENT_LIST_DIR}/oat/${source}")
endforeach()
file(GLOB_RECURSE KISAK_ZONE_GENERATED_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/generated/*.cpp")

add_library(kisakcod_zoneload STATIC ${KISAK_OAT_SOURCES} ${KISAK_ZONE_GENERATED_SOURCES}
    "${CMAKE_CURRENT_LIST_DIR}/bridge/kisak_zone_bridge.cpp")
target_compile_features(kisakcod_zoneload PRIVATE cxx_std_23)
# ARCH_x64 selects OAT's 64-bit host code paths (32-bit zones into 64-bit structs).
target_compile_definitions(kisakcod_zoneload PUBLIC ARCH_x64)
target_include_directories(kisakcod_zoneload PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/generated"
    "${CMAKE_CURRENT_LIST_DIR}/oat/Common"
    "${CMAKE_CURRENT_LIST_DIR}/oat/ZoneCommon"
    "${CMAKE_CURRENT_LIST_DIR}/oat/ZoneLoading"
    "${CMAKE_CURRENT_LIST_DIR}/oat/Utils"
    "${CMAKE_CURRENT_LIST_DIR}/bridge"
)
