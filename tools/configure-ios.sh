#!/bin/bash
set -euo pipefail
source_root="$(cd "$(dirname "$0")/.." && pwd)"
build_root="${COD4IOS_BUILD_DIR:-$source_root/build/ios/xcode-engine}"
cmake -S "$source_root" -B "$build_root" -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT="${COD4IOS_SDK:-iphoneos}" \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DCMAKE_XCODE_GENERATE_SCHEME=ON \
  -DKISAK_BUILD_ENGINE=ON -DKISAK_BUILD_MP=ON -DKISAK_BUILD_COMBINED=ON \
  -DKISAK_IOS_TEAM="${KISAK_IOS_TEAM:-}" \
  -DKISAK_COMBINED_BUNDLE_ID="${COD4IOS_BUNDLE_ID:-com.devz.cod4ios}" \
  -DFETCHCONTENT_SOURCE_DIR_OPENAL="$source_root/third-party/openal-soft"
printf '\nOpen %s/KisakCOD.xcodeproj\n' "$build_root"
