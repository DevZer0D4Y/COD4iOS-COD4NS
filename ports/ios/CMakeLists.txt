add_subdirectory(core)
add_subdirectory(assets)
add_subdirectory(save)
if(APPLE)
    add_subdirectory(platform)
    add_subdirectory(script)
    option(KISAK_BUILD_ENGINE "Build the single-player engine library" OFF)
    if(KISAK_BUILD_ENGINE)
        add_subdirectory(engine)
    endif()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(kisakcod_cod4x_transport_tests network/Cod4xTransportTests.cpp network/cod4x_transport.cpp)
    target_compile_features(kisakcod_cod4x_transport_tests PRIVATE cxx_std_17)
    target_compile_options(kisakcod_cod4x_transport_tests PRIVATE -UNDEBUG)
    add_test(NAME ios_cod4x_transport COMMAND kisakcod_cod4x_transport_tests)
    add_executable(kisakcod_cod4x_compatibility_tests network/Cod4xCompatibilityTests.cpp)
    target_compile_features(kisakcod_cod4x_compatibility_tests PRIVATE cxx_std_17)
    target_compile_options(kisakcod_cod4x_compatibility_tests PRIVATE -UNDEBUG)
    add_test(NAME ios_cod4x_compatibility COMMAND kisakcod_cod4x_compatibility_tests)
    add_executable(kisakcod_huffman_wire_tests network/HuffmanWireTests.cpp ../../src/qcommon/huffman.cpp)
    target_include_directories(kisakcod_huffman_wire_tests PRIVATE ../../src)
    target_compile_features(kisakcod_huffman_wire_tests PRIVATE cxx_std_17)
    target_compile_options(kisakcod_huffman_wire_tests PRIVATE -UNDEBUG)
    add_test(NAME ios_huffman_wire COMMAND kisakcod_huffman_wire_tests)
    add_executable(kisakcod_shader_constant_tests input/ShaderConstantTests.cpp)
    target_compile_features(kisakcod_shader_constant_tests PRIVATE cxx_std_17)
    target_compile_options(kisakcod_shader_constant_tests PRIVATE -UNDEBUG)
    add_test(NAME ios_shader_constants COMMAND kisakcod_shader_constant_tests)
    add_executable(kisakcod_controller_tests input/ControllerTests.cpp)
    target_compile_features(kisakcod_controller_tests PRIVATE cxx_std_17)
    target_compile_options(kisakcod_controller_tests PRIVATE -UNDEBUG)
    add_test(NAME ios_controller COMMAND kisakcod_controller_tests)
    add_executable(kisakcod_camera_tests input/CameraTests.cpp)
    target_compile_features(kisakcod_camera_tests PRIVATE cxx_std_17)
    # The camera takes its basis from the engine's AngleVectors().
    target_link_libraries(kisakcod_camera_tests PRIVATE kisakcod_core)
    add_test(NAME ios_camera COMMAND kisakcod_camera_tests)
    return()
endif()

enable_language(OBJCXX)

if(KISAK_BUILD_ENGINE)
    # The single-player engine app. Game data is copied by the user into the
    # app's Documents folder (file sharing is enabled in Info.plist.in).
    add_executable(KisakCOD-SP MACOSX_BUNDLE app/engine_app.mm)
    target_compile_features(KisakCOD-SP PRIVATE cxx_std_20)
    target_compile_options(KisakCOD-SP PRIVATE -fobjc-arc)
    target_link_libraries(KisakCOD-SP PRIVATE kisakcod_engine_sp
        "-framework Foundation" "-framework UIKit" "-framework QuartzCore" "-framework Metal"
        "-framework CoreAudio" "-framework AudioToolbox" "-framework AVFoundation" "-framework GameController")
    set(KISAK_SP_BUNDLE_ID "org.kisakcod.sp" CACHE STRING "Engine app bundle identifier")
    set_target_properties(KisakCOD-SP PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/app/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${KISAK_SP_BUNDLE_ID}"
        MACOSX_BUNDLE_BUNDLE_NAME "KisakCOD"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${KISAK_SP_BUNDLE_ID}"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${KISAK_IOS_TEAM}"
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
    )
endif()
# The multiplayer app, built from the same shell as the SP one but linked against the MP engine.
# Separate bundle id so both can be installed side by side.
if(KISAK_BUILD_ENGINE AND KISAK_BUILD_MP)
    add_executable(KisakCOD-MP MACOSX_BUNDLE app/engine_app.mm)
    target_compile_features(KisakCOD-MP PRIVATE cxx_std_20)
    target_compile_options(KisakCOD-MP PRIVATE -fobjc-arc)
    target_compile_definitions(KisakCOD-MP PRIVATE KISAK_MP)
    target_link_libraries(KisakCOD-MP PRIVATE kisakcod_engine_mp
        "-framework Foundation" "-framework UIKit" "-framework QuartzCore" "-framework Metal"
        "-framework CoreAudio" "-framework AudioToolbox" "-framework AVFoundation" "-framework GameController")
    set(KISAK_MP_BUNDLE_ID "org.kisakcod.mp" CACHE STRING "Multiplayer app bundle identifier")
    set_target_properties(KisakCOD-MP PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/app/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${KISAK_MP_BUNDLE_ID}"
        MACOSX_BUNDLE_BUNDLE_NAME "KisakCOD MP"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${KISAK_MP_BUNDLE_ID}"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${KISAK_IOS_TEAM}"
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
    )
endif()

# One app carrying both engines. Each is a dylib exporting only KisakEngine_AppMain, because
# KISAK_MP changes struct layouts and the two cannot share a binary; iOS two-level namespacing
# keeps each image's globals private and the launcher loads exactly one per run.
if(KISAK_BUILD_ENGINE AND KISAK_BUILD_MP AND KISAK_BUILD_COMBINED)
    foreach(mode sp mp)
        add_library(kisakcod_${mode}_engine SHARED app/engine_app.mm)
        set_target_properties(kisakcod_${mode}_engine PROPERTIES
            OUTPUT_NAME "kisakcod_${mode}"
            XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${KISAK_IOS_TEAM}"
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
            BUILD_WITH_INSTALL_NAME_DIR TRUE
            INSTALL_NAME_DIR "@executable_path/Frameworks")
        target_compile_features(kisakcod_${mode}_engine PRIVATE cxx_std_20)
        target_compile_definitions(kisakcod_${mode}_engine PRIVATE KISAK_ENGINE_AS_DYLIB)
        target_link_libraries(kisakcod_${mode}_engine PRIVATE
            "-framework Foundation" "-framework UIKit" "-framework QuartzCore" "-framework Metal"
            "-framework CoreAudio" "-framework AudioToolbox" "-framework AVFoundation" "-framework GameController")
    endforeach()
    target_compile_definitions(kisakcod_mp_engine PRIVATE KISAK_MP)
    target_link_libraries(kisakcod_sp_engine PRIVATE kisakcod_engine_sp)
    target_link_libraries(kisakcod_mp_engine PRIVATE kisakcod_engine_mp)

    add_executable(KisakCOD-Combined MACOSX_BUNDLE app/launcher.mm)
    target_sources(KisakCOD-Combined PRIVATE app/Assets.xcassets)
    set_source_files_properties(app/Assets.xcassets PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_compile_features(KisakCOD-Combined PRIVATE cxx_std_20)
    set(KISAK_COMBINED_BUNDLE_ID "com.devz.cod4ios" CACHE STRING "Combined app bundle identifier")
    set_target_properties(KisakCOD-Combined PROPERTIES
        OUTPUT_NAME "KisakCOD"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/app/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${KISAK_COMBINED_BUNDLE_ID}"
        MACOSX_BUNDLE_BUNDLE_NAME "COD4iOS"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.3"
        MACOSX_BUNDLE_BUNDLE_VERSION "4"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${KISAK_COMBINED_BUNDLE_ID}"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${KISAK_IOS_TEAM}"
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
        XCODE_EMBED_FRAMEWORKS "kisakcod_sp_engine;kisakcod_mp_engine"
        XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY "YES")
    target_link_libraries(KisakCOD-Combined PRIVATE "-framework Foundation" "-framework UIKit")
    add_dependencies(KisakCOD-Combined kisakcod_sp_engine kisakcod_mp_engine)
endif()

set(KISAK_IOS_BUNDLE_ID "org.kisakcod.iospreview" CACHE STRING "App bundle identifier")
set(KISAK_IOS_TEAM "" CACHE STRING "Apple development team for device signing")

add_executable(KisakCOD-iOS MACOSX_BUNDLE
    app/main.mm
    app/MapViewController.mm
    app/MetalRenderer.mm
)
set(KISAK_IOS_PREVIEW_MAP "" CACHE FILEPATH "Optional user-owned .ff/.d3dbsp map for a personal build")
if(KISAK_IOS_PREVIEW_MAP)
    if(NOT EXISTS "${KISAK_IOS_PREVIEW_MAP}")
        message(FATAL_ERROR "KISAK_IOS_PREVIEW_MAP does not exist")
    endif()
    target_sources(KisakCOD-iOS PRIVATE "${KISAK_IOS_PREVIEW_MAP}")
    set_source_files_properties("${KISAK_IOS_PREVIEW_MAP}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()
target_compile_features(KisakCOD-iOS PRIVATE cxx_std_17)
target_compile_options(KisakCOD-iOS PRIVATE -fobjc-arc -Wall -Wextra)
target_link_libraries(KisakCOD-iOS PRIVATE kisakcod_core kisakcod_assets objc
    "-framework Foundation"
    "-framework UIKit" "-framework Metal" "-framework MetalKit"
    "-framework GameController" "-framework UniformTypeIdentifiers"
    "-framework QuartzCore" "-framework CoreGraphics")
set_target_properties(KisakCOD-iOS PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/app/Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${KISAK_IOS_BUNDLE_ID}"
    MACOSX_BUNDLE_BUNDLE_NAME "KisakCOD Preview"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${KISAK_IOS_BUNDLE_ID}"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${KISAK_IOS_TEAM}"
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
    XCODE_ATTRIBUTE_ENABLE_USER_SCRIPT_SANDBOXING "YES"
)

# Shared native touch overlay for both standalone and combined engine shells.
foreach(app_target KisakCOD-SP KisakCOD-MP kisakcod_sp_engine kisakcod_mp_engine)
    if(TARGET ${app_target})
        target_sources(${app_target} PRIVATE app/touch_controls.mm app/client_patch.mm app/TouchControls.LICENSE)
        target_link_libraries(${app_target} PRIVATE "-framework CoreGraphics")
        set_source_files_properties(app/TouchControls.LICENSE PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    endif()
endforeach()
if(TARGET KisakCOD-Combined)
    target_sources(KisakCOD-Combined PRIVATE app/TouchControls.LICENSE)
endif()
