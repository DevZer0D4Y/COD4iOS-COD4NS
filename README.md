# COD4iOS

An experimental iPhone/iPad port of [KisakCOD](https://github.com/SwagSoftware/KisakCOD), with separate single-player and multiplayer engines in one app. This source snapshot corresponds to **1.0.3, build 4**.

The repository contains source code and required third-party source dependencies. It does not include Call of Duty 4 retail game files, movies, player profiles, saves, accounts, signing certificates or provisioning profiles. Use your own PC game installation.

## Features

- Campaign and CoD4x multiplayer support, including the current asset/loading, transport, custom-map, spawn/camera and HUD corrections.
- Touch controls derived from MC360-Recomp and Xbox-style controller bindings/icons. Touch controls hide when a controller connects and return when it disconnects.
- Controller/touch aim assist, reduced sensitivity while aiming, click-to-sprint and a toggle scoreboard.
- Automatic SHA-256-verified download of the public CoD4x compatibility patch.
- A filtered server browser that also works with empty Documents: 26 public endpoints independently received CoD4x gamestates without Steam on 3 October 2026. Explicit authentication rejections override that baseline locally; unknown endpoints are hidden by default.
- Uncapped engine frame rate and support for the display's maximum refresh rate. Actual FPS depends on device performance and the display.

This is a community port under active testing. Receipt of a multiplayer gamestate does not establish complete gameplay on every server. Servers requiring Steam or official-client hardware authentication remain incompatible; filtering does not supply those credentials. Server policies can change.

## Build

Requirements: macOS, Xcode with the iPhoneOS SDK and C++23 support, CMake 3.24 or newer, and Python 3 for the release tools. The 1.0.3 app was built with Xcode 27.0; the deployment target is iOS 17.0. The OpenAL Soft 1.25.2 source is included in `third-party/openal-soft`.

```sh
bash tools/configure-ios.sh
open build/ios/xcode-engine/KisakCOD.xcodeproj
```

In Xcode, choose the `KisakCOD-Combined` target/scheme, select your own development team and device, then build/run. The internal KisakCOD target names are retained from upstream; the installed app is **COD4iOS**, bundle ID **com.devz.cod4ios**. A different developer can configure their own bundle identifier and signing team; see [the build guide](docs/BUILDING.md).

For an unsigned Release build:

```sh
xcodebuild -project build/ios/xcode-engine/KisakCOD.xcodeproj \
  -target KisakCOD-Combined -configuration Release -sdk iphoneos \
  -jobs 1 CODE_SIGNING_ALLOWED=NO build
```

Both embedded engine dylibs must be signed when installing on a physical device. Signing credentials are never supplied by this repository.

## Game data

Open the app once, then use Finder File Sharing to copy `localization.txt`, `main/` and `zone/` from your own PC installation into COD4iOS Documents. Preserve the directory structure and reopen the app. Allow internet access for the CoD4x patch bootstrap on the first multiplayer launch.

Campaign/menu movies use converted MP4 files alongside the original BIK movies. If needed, convert your own movies with ffmpeg:

```sh
bash ports/ios/scripts/convert_videos.sh "/path/to/Call of Duty 4" all
```

## Checks and releases

```sh
python3 tools/check-public-source.py
cmake -S . -B build/host -DKISAK_BUILD_PORT_TESTS=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host --parallel 1
ctest --test-dir build/host --output-on-failure
```

See [the build guide](docs/BUILDING.md) for the multiplayer regression checks and packaging command, and [the publishing guide](docs/PUBLISHING.md) for uploading this source tree and the separate IPA to GitHub.

For a bug report, include device/iOS version, mission or server/map, reproduction steps and the error text. Review `kisakcod.log` before attaching it: runtime logs can contain player names, server addresses and activity. Do not attach game folders or signing credentials.

## Credits and licensing

The project retains the upstream [GPLv3 license](LICENSE) and original source notices. See [third-party notices](NOTICE.md) for OpenAssetTools, DXVK/MinGW headers, OpenAL Soft and the MC360-Recomp touch layout. Original KisakCOD information and credits are preserved in [the upstream README](docs/UPSTREAM_README.md). Call of Duty and its artwork belong to their respective owners; this project is not an official Activision release.
