#!/bin/bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../../.." && pwd)"
build_root="${KISAK_BUILD_ROOT:-$repo_root/build/ios}"
action="${1:-configure}"
# std::format (used by the engine) needs std::to_chars: iOS 16.3 or later.
deployment_target="${KISAK_IOS_MIN_VERSION:-17.0}"

configure_apple() {
    local sdk="$1"
    local generator="$2"
    local destination="$3"
    # Framework paths cached by FindZLIB/OpenAL refer to the old SDK after an Xcode update.
    # Re-discover just those paths; retain compiled objects, provisioning and user options.
    cmake -S "$repo_root" -B "$build_root/$destination" -G "$generator" \
        -U 'ZLIB_*' -U '*FRAMEWORK' -U 'AUDIOTOOLBOX_LIBRARY' -U 'AUDIOUNIT_INCLUDE_DIR' \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_C_COMPILER="$(xcrun --find clang)" \
        -DCMAKE_CXX_COMPILER="$(xcrun --find clang++)" \
        -DCMAKE_OBJCXX_COMPILER="$(xcrun --find clang++)" \
        -DCMAKE_OSX_SYSROOT="$sdk" \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="$deployment_target" \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_BUILD_TYPE=Release \
        -DKISAK_IOS_TEAM="${KISAK_IOS_TEAM:-}" \
        -DKISAK_IOS_PREVIEW_MAP="${KISAK_IOS_PREVIEW_MAP:-}" \
        -DKISAK_IOS_BUNDLE_ID="${KISAK_IOS_BUNDLE_ID:-org.kisakcod.iospreview}"
}

case "$action" in
    configure)
        configure_apple iphoneos Xcode xcode
        printf '\nProgetto Xcode: %s/xcode/KisakCOD.xcodeproj\n' "$build_root"
        ;;
    device|simulator)
        sdk=iphoneos
        if [[ "$action" == simulator ]]; then sdk=iphonesimulator; fi
        configure_apple "$sdk" 'Unix Makefiles' "$sdk"
        # Build proof only: device installation needs a development team/profile.
        cmake --build "$build_root/$sdk" --config Release \
            --target KisakCOD-iOS kisakcod_script_support kisakcod_script_variables kisakcod_save_support --parallel
        ;;
    test)
        cmake -S "$repo_root" -B "$build_root/host" \
            -DKISAK_BUILD_PORT_TESTS=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer" \
            -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
        cmake --build "$build_root/host" --parallel
        # -fno-sanitize-recover makes undefined behaviour abort: without it the
        # sanitizer only prints and the test still reports success.
        ctest --test-dir "$build_root/host" --output-on-failure
        ;;
    zoneload-ios)
        # Only the 64-bit fastfile loader library, built with the iPhone SDK.
        configure_apple iphoneos 'Unix Makefiles' iphoneos-engine
        cmake -S "$repo_root" -B "$build_root/iphoneos-engine" -DKISAK_BUILD_ENGINE=ON
        nice -n 19 cmake --build "$build_root/iphoneos-engine" --target kisakcod_zoneload \
            --parallel "${KISAK_BUILD_JOBS:-2}" -- -k
        ;;
    engine-ios)
        # The single-player engine library built with the iPhone SDK.
        configure_apple iphoneos 'Unix Makefiles' iphoneos-engine
        cmake -S "$repo_root" -B "$build_root/iphoneos-engine" -DKISAK_BUILD_ENGINE=ON
        # Keep the machine usable: a few low-priority jobs, never unbounded.
        nice -n 19 cmake --build "$build_root/iphoneos-engine" --target kisakcod_engine_sp \
            --parallel "${KISAK_BUILD_JOBS:-2}" -- -k
        ;;
    sim-app)
        # The engine app for the iOS Simulator (no signing needed).
        configure_apple iphonesimulator 'Unix Makefiles' iphonesimulator-engine
        cmake -S "$repo_root" -B "$build_root/iphonesimulator-engine" -DKISAK_BUILD_ENGINE=ON
        nice -n 19 cmake --build "$build_root/iphonesimulator-engine" --target KisakCOD-SP \
            --parallel "${KISAK_BUILD_JOBS:-2}"
        ;;
    sim-app-mp)
        # Multiplayer app (experimental): same engine sources built with KISAK_MP.
        # configure_apple first: without it a fresh build dir has no CMAKE_SYSTEM_NAME
        # and CMake configures for win32, which the Makefiles generator rejects.
        configure_apple iphonesimulator 'Unix Makefiles' iphonesimulator-engine
        cmake -S "$repo_root" -B "$build_root/iphonesimulator-engine" -DKISAK_BUILD_ENGINE=ON -DKISAK_BUILD_MP=ON
        nice -n 19 cmake --build "$build_root/iphonesimulator-engine" --target KisakCOD-MP \
            --parallel "${KISAK_BUILD_JOBS:-2}"
        ;;
    sim-run)
        # Install and launch in the simulator. The user's game data is linked, not
        # copied: main/ is a real folder of links so files the engine writes there
        # (profiles, config) stay inside the app container.
        device="${KISAK_SIM_DEVICE:-iPhone 17 Pro}"
        game="${KISAK_GAME_DIR:?Set KISAK_GAME_DIR to the Call of Duty 4 folder}"
        # KISAK_SIM_APP=mp runs the multiplayer app instead of single-player.
        if [[ "${KISAK_SIM_APP:-sp}" == "mp" ]]; then
            app_name="KisakCOD-MP.app"; bundle_id="org.kisakcod.mp"
        else
            app_name="KisakCOD-SP.app"; bundle_id="org.kisakcod.sp"
        fi
        app="$(find "$build_root/iphonesimulator-engine" -name "$app_name" -type d | head -1)"
        [[ -n "$app" ]] || { printf 'Build the simulator app first: %s sim-app\n' "$0" >&2; exit 1; }
        xcrun simctl boot "$device" 2>/dev/null || true
        # Installing gives the app a brand new data container, so carry the state the player
        # would not want to recreate: their profile, and the learned CoD4x server verdicts.
        carry="$(mktemp -d)"
        old_documents="$(xcrun simctl get_app_container "$device" "$bundle_id" data 2>/dev/null || true)/Documents"
        if [[ -d "$old_documents" ]]; then
            [[ -d "$old_documents/players" ]] && cp -R "$old_documents/players" "$carry/"
            [[ -f "$old_documents/cod4x_servers.txt" ]] && cp "$old_documents/cod4x_servers.txt" "$carry/"
        fi
        xcrun simctl install "$device" "$app"
        documents="$(xcrun simctl get_app_container "$device" "$bundle_id" data)/Documents"
        mkdir -p "$documents"
        [[ -d "$carry/players" ]] && cp -R "$carry/players" "$documents/"
        [[ -f "$carry/cod4x_servers.txt" ]] && cp "$carry/cod4x_servers.txt" "$documents/"
        rm -rf "$carry"
        mkdir -p "$documents/main"
        ln -sfn "$game/localization.txt" "$documents/localization.txt"
        # zone/ is a real folder tree of per-file links, like main/: the app sandbox
        # refuses a directory symlink that leaves the container.
        if [[ -L "$documents/zone" ]]; then rm "$documents/zone"; fi
        for langdir in "$game"/zone/*/; do
            lang="$(basename "$langdir")"
            mkdir -p "$documents/zone/$lang"
            for fastfile in "$langdir"*.ff; do
                ln -sfn "$fastfile" "$documents/zone/$lang/$(basename "$fastfile")"
            done
        done
        # CoD4x servers reference their own patch fastfiles; its installer drops them beside the
        # Windows client rather than in the game folder. DB_BuildOSPath looks in zone/<language>/,
        # so link them next to the stock zones when they are present.
        cod4x_zone="${KISAK_COD4X_ZONE:-$game/../../../../../users/crossover/AppData/Local/CallofDuty4MW/zone}"
        if [[ -d "$cod4x_zone" ]]; then
            for langdir in "$documents"/zone/*/; do
                for fastfile in "$cod4x_zone"/*.ff; do
                    [[ -e "$fastfile" ]] || continue
                    ln -sfn "$fastfile" "$langdir$(basename "$fastfile")"
                done
            done
        fi
        for archive in "$game"/main/*.iwd; do
            ln -sfn "$archive" "$documents/main/$(basename "$archive")"
        done
        # Loose .cfg files from the player's own main/ (kisak_local.cfg and friends).
        for config in "$game"/main/*.cfg; do
            [[ -e "$config" ]] || continue
            ln -sfn "$config" "$documents/main/$(basename "$config")"
        done
        if [[ -d "$game/main/video" ]]; then ln -sfn "$game/main/video" "$documents/main/video"; fi
        xcrun simctl terminate "$device" "$bundle_id" 2>/dev/null || true
        xcrun simctl launch "$device" "$bundle_id"
        printf 'Engine log: %s/kisakcod.log\n' "$documents"
        ;;
    app-ios)
        # Signed engine app for a connected iPhone (Xcode generator for automatic signing).
        cmake -S "$repo_root" -B "$build_root/xcode-engine" -G Xcode \
            -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
            -DCMAKE_OSX_DEPLOYMENT_TARGET="$deployment_target" \
            -DKISAK_BUILD_ENGINE=ON -DKISAK_IOS_TEAM="${KISAK_IOS_TEAM:-}"
        nice -n 19 xcodebuild -project "$build_root/xcode-engine/KisakCOD.xcodeproj" -scheme KisakCOD-SP \
            -configuration Release -destination "generic/platform=iOS" -allowProvisioningUpdates \
            -jobs "${KISAK_BUILD_JOBS:-2}" build
        ;;
    app-ios-combined)
        # One app with both engines. Switching mode takes effect on the next launch.
        cmake -S "$repo_root" -B "$build_root/xcode-engine" -G Xcode \
            -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
            -DCMAKE_OSX_DEPLOYMENT_TARGET="$deployment_target" \
            -DKISAK_BUILD_ENGINE=ON -DKISAK_BUILD_MP=ON -DKISAK_BUILD_COMBINED=ON \
            -DKISAK_IOS_TEAM="${KISAK_IOS_TEAM:?Set KISAK_IOS_TEAM to your Apple development team id}"
        nice -n 19 xcodebuild -project "$build_root/xcode-engine/KisakCOD.xcodeproj" -scheme KisakCOD-Combined \
            -configuration Release -destination "generic/platform=iOS" -allowProvisioningUpdates \
            -jobs "${KISAK_BUILD_JOBS:-2}" build
        printf '\nApp: %s\n' "$(find "$build_root/xcode-engine" -name 'KisakCOD.app' -type d | head -1)"
        ;;
    app-ios-mp)
        # Signed multiplayer app for a connected iPhone. Needs a development team:
        # KISAK_IOS_TEAM, or the 10-character id in your signing certificate's name.
        cmake -S "$repo_root" -B "$build_root/xcode-engine" -G Xcode \
            -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
            -DCMAKE_OSX_DEPLOYMENT_TARGET="$deployment_target" \
            -DKISAK_BUILD_ENGINE=ON -DKISAK_BUILD_MP=ON -DKISAK_IOS_TEAM="${KISAK_IOS_TEAM:?Set KISAK_IOS_TEAM to your Apple development team id}"
        nice -n 19 xcodebuild -project "$build_root/xcode-engine/KisakCOD.xcodeproj" -scheme KisakCOD-MP \
            -configuration Release -destination "generic/platform=iOS" -allowProvisioningUpdates \
            -jobs "${KISAK_BUILD_JOBS:-2}" build
        printf '\nApp: %s\n' "$(find "$build_root/xcode-engine" -name 'KisakCOD-MP.app' -type d | head -1)"
        ;;
    device-install)
        # Install the signed multiplayer app on the connected iPhone.
        device="${KISAK_DEVICE:?Set KISAK_DEVICE to the iPhone UDID (xcrun devicectl list devices)}"
        app="$(find "$build_root/xcode-engine" -name 'KisakCOD.app' -type d | head -1)"
        [[ -n "$app" ]] || app="$(find "$build_root/xcode-engine" -name 'KisakCOD-MP.app' -type d | head -1)"
        [[ -n "$app" ]] || { printf 'Build it first: %s app-ios-combined\n' "$0" >&2; exit 1; }
        xcrun devicectl device install app --device "$device" "$app"
        ;;
    device-data)
        # Push the game data into the app's Documents. Only what multiplayer needs: every
        # main/*.iwd, the mp and shared zones, and the CoD4x patch fastfiles. Unchanged files
        # are skipped, so re-running this is cheap.
        device="${KISAK_DEVICE:?Set KISAK_DEVICE to the iPhone UDID}"
        game="${KISAK_GAME_DIR:?Set KISAK_GAME_DIR to the Call of Duty 4 folder}"
        [[ -d "$game/main" ]] || { printf 'No game data at %s\n' "$game" >&2; exit 1; }
        bundle="${KISAK_MP_BUNDLE_ID:-org.kisakcod.mp}"
        # Stage with hard links where we can: the game data is several GB and lives on the
        # same volume, so linking costs nothing and a plain copy would need it twice over.
        link() { cp -l "$1" "$2" 2>/dev/null || cp "$1" "$2"; }
        staging="$(mktemp -d)"
        trap 'rm -rf "$staging"' EXIT
        mkdir -p "$staging/main"
        for archive in "$game"/main/*.iwd; do link "$archive" "$staging/main/"; done
        for config in "$game"/main/*.cfg; do [[ -e "$config" ]] && link "$config" "$staging/main/"; done
        # The menus play these; without them the engine stalls looking for the intro movies.
        if [[ -d "$game/main/video" ]]; then
            mkdir -p "$staging/main/video"
            # Only the boot movies, and only the converted ones: there is no Bink runtime on iOS,
            # so cinematic_apple.cpp plays video/<name>.mp4. Run convert_videos.sh first.
            for movie in "$game"/main/video/{IW_logo,atvi,cod_intro}.mp4; do
                [[ -f "$movie" ]] && link "$movie" "$staging/main/video/"
            done
            if ! compgen -G "$staging/main/video/*.mp4" >/dev/null; then
                printf 'No converted movies; run ports/ios/scripts/convert_videos.sh "%s" IW_logo atvi cod_intro\n' "$game" >&2
            fi
        fi
        link "$game/localization.txt" "$staging/" 2>/dev/null || true
        # Carry forward anything we saved from earlier runs (learned server verdicts, profile).
        stage_extra="$build_root/device-stage"
        if [[ -d "$stage_extra" ]]; then cp -R "$stage_extra"/* "$staging/" 2>/dev/null || true; fi
        for langdir in "$game"/zone/*/; do
            lang="$(basename "$langdir")"
            mkdir -p "$staging/zone/$lang"
            # Campaign fastfiles are dead weight on a multiplayer-only device.
            for fastfile in "$langdir"{mp_*,*_mp,common,localized_common,ui,localized_ui,code_post_gfx,localized_code_post_gfx,simplecredits}.ff; do
                [[ -e "$fastfile" ]] && link "$fastfile" "$staging/zone/$lang/"
            done
            cod4x_zone="${KISAK_COD4X_ZONE:-$game/../../../../../users/crossover/AppData/Local/CallofDuty4MW/zone}"
            if [[ -d "$cod4x_zone" ]]; then
                for fastfile in "$cod4x_zone"/*.ff; do
                    [[ -e "$fastfile" ]] && link "$fastfile" "$staging/zone/$lang/"
                done
            fi
        done
        printf 'Staged %s, copying to the device...\n' "$(du -sh "$staging" | cut -f1)"
        for item in "$staging"/*; do
            xcrun devicectl device copy to --device "$device" --domain-type appDataContainer \
                --domain-identifier "$bundle" --source "$item" --destination "Documents/$(basename "$item")"
        done
        ;;
    device-log)
        # Pull the engine log and any crash reports off the phone.
        device="${KISAK_DEVICE:?Set KISAK_DEVICE to the iPhone UDID}"
        bundle="${KISAK_MP_BUNDLE_ID:-org.kisakcod.mp}"
        out="${KISAK_DEVICE_LOG_DIR:-$build_root/device-logs}"
        mkdir -p "$out"
        xcrun devicectl device copy from --device "$device" --domain-type appDataContainer \
            --domain-identifier "$bundle" --source "Documents/kisakcod.log" --destination "$out/kisakcod.log" || true
        xcrun devicectl device copy from --device "$device" --domain-type systemCrashLogs \
            --source . --destination "$out/crashes" || true
        printf 'Wrote %s\n' "$out"
        ;;
    engine)
        # macOS arm64 host build of the real engine: same ABI as the iPhone,
        # faster to iterate and debug. -k keeps going to report every error.
        cmake -S "$repo_root" -B "$build_root/host-engine" \
            -DKISAK_BUILD_PORT_TESTS=ON -DKISAK_BUILD_ENGINE=ON -DBUILD_TESTING=OFF \
            -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64
        # Keep the machine usable: a few low-priority jobs, never unbounded.
        nice -n 19 cmake --build "$build_root/host-engine" --target kisakcod_sp_host \
            --parallel "${KISAK_BUILD_JOBS:-2}" -- -k
        ;;
    *)
        printf 'Uso: %s {configure|device|simulator|test|zoneload-ios|engine-ios|app-ios|sim-app|sim-run|engine}\n' "$0" >&2
        exit 2
        ;;
esac
