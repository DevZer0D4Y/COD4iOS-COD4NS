# Building COD4iOS

Use macOS, CMake 3.24+, Python 3 and an Xcode installation that supports C++23 and the iPhoneOS SDK. The release was built with Xcode 27.0 and targets iOS 17.0+. Internal target names retain KisakCOD for build compatibility.

## Configure and sign

```sh
bash tools/configure-ios.sh
```

The helper uses the included OpenAL Soft source rather than fetching another checkout. Optional environment variables:

| Variable | Purpose |
| --- | --- |
| `KISAK_IOS_TEAM` | Your Apple development team ID; no team is bundled |
| `COD4IOS_BUNDLE_ID` | Override `com.devz.cod4ios` when your provisioning setup needs another identifier |
| `COD4IOS_BUILD_DIR` | Override `build/ios/xcode-engine` |
| `COD4IOS_SDK` | `iphoneos` by default; `iphonesimulator` configures a separate simulator build directory |

Open the generated Xcode project, select `KisakCOD-Combined`, choose your team and device, then build/run. One compiler job is recommended on Macs with limited RAM. The app loads one of two separate engine dylibs, so a sideloading tool must sign both of them as well as the main executable.

For an unsigned device build:

```sh
xcodebuild -project build/ios/xcode-engine/KisakCOD.xcodeproj \
  -target KisakCOD-Combined -configuration Release -sdk iphoneos \
  -jobs 1 CODE_SIGNING_ALLOWED=NO build
```

The app is `build/ios/xcode-engine/ports/ios/Release-iphoneos/KisakCOD.app`. A simulator build is not installable on an iPhone and cannot be used as a device IPA.

## Package a public IPA

```sh
python3 tools/package-public-playtest.py \
  --app build/ios/xcode-engine/ports/ios/Release-iphoneos/KisakCOD.app \
  --source-root "$PWD" --output release/public
```

The packager copies only app resources and both engines, removes provisioning/signing material, strips build-machine source paths, verifies exported symbols and creates an unsigned IPA plus matching source archive. Game files, profiles, saves and mod caches are excluded. It temporarily verifies an ad-hoc signature and removes it before packaging. Install using your own signing credentials.

## Host regressions

```sh
cmake -S . -B build/host -DKISAK_BUILD_PORT_TESTS=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host --parallel 1
ctest --test-dir build/host --output-on-failure
python3 ports/ios/engine/tests/ServerBrowserTests.py
python3 ports/ios/engine/tests/ClientIdentityTests.py
python3 ports/ios/engine/tests/CoD4xDownloadTests.py
clang++ -std=c++17 ports/ios/network/ServerFilterTests.cpp -o /tmp/cod4ios-filter-test
/tmp/cod4ios-filter-test
```

These checks do not require retail data and are not substitutes for campaign or multiplayer gameplay tests. `ClientPatchTests.mm` additionally accepts a separately obtained official patch fixture; `--network` tests the first HTTPS bootstrap. No fixture fastfile is distributed.

## Game files and cinematics

Copy your own PC installation's `localization.txt`, `main/` and `zone/` into the app's Documents using Finder File Sharing. Keep the existing profile/saves when updating an installed app. Convert your own BIK cinematics to MP4 with `ports/ios/scripts/convert_videos.sh`; ffmpeg is required. Do not commit these files.
