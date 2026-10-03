# COD4iOS-COD4NS --> For now, only COD4iOS is available.

Call of Duty 4: Modern Warfare on iPhone and iPad, based on [KisakCOD](https://github.com/SwagSoftware/KisakCOD).

COD4iOS includes single-player and CoD4x multiplayer in one app, with touch controls and Xbox-style controller support. This source release is **1.0.3, build 4**. The port is still in development, so expect bugs and differences between devices and servers.

**You need your own copy of the original PC game, Call of Duty 4: Modern Warfare (2007).** Modern Warfare Remastered files will not work. The source and IPA do not include the retail game data.

If you want to support me and remain up-to-date about this and other projects, you can in different ways:

Ko-Fi: https://ko-fi.com/dev_zer0

Discord: https://discord.gg/uFChheZEWX

YouTube: https://www.youtube.com/@develop_erZ

## What you need

- An iPhone or iPad running iOS/iPadOS 17 or later.
- Your PC game's `main` folder, `zone` folder and `localization.txt` file.
- Enough free storage for those files, converted cutscenes, saves and downloaded maps. Check the size of your game folders before transferring them.
- A Mac or Windows PC to sign/install the release IPA and transfer the files. Building from source requires a Mac.
- Internet access for multiplayer and the first CoD4x patch download.

## Install the release IPA

Download the `.ipa` attached to a GitHub release. The public IPA is **unsigned**: it needs to be signed before an iPhone can run it. Opening it in the Files app does not install it.

You can sign and install it with [Sideloadly](https://sideloadly.io/), available for macOS and Windows:

1. Install Sideloadly and follow its setup instructions for your computer.
2. Connect your iPhone by USB, unlock it and accept **Trust This Computer** if asked.
3. Select the **iPhone** in Sideloadly, load the COD4iOS IPA and sign in with the Apple Account you use for sideloading.
4. Start the installation and complete any account verification prompts.
5. If iOS asks you to trust the developer, open **Settings → General → VPN & Device Management** and trust the account used to sign the app.
6. Enable **Settings → Privacy & Security → Developer Mode** if required, then restart and confirm it. See [Apple's Developer Mode instructions](https://developer.apple.com/documentation/xcode/enabling-developer-mode-on-a-device).
7. Open COD4iOS once. A game-files message is expected until you copy the data below.

The app contains both single-player and multiplayer engine libraries. Any signing tool you use must sign the embedded libraries as well as the app.

Free-account installations normally expire after seven days. Refresh or reinstall using the same account and bundle identifier; keep a backup of your saves. See the [Sideloadly FAQ](https://sideloadly.io/faq.html) for signing, refresh and installation help.

## Copy the game files

Find the installation folder on your computer. For a Steam copy, use **Manage → Browse local files** from the game's library entry.

Copy these three items:

| Item | What to copy |
| --- | --- |
| `localization.txt` | The file from the root of your PC game installation. |
| `main/` | The whole folder, including every `.iwd` archive, localized archives and `video/`. |
| `zone/` | The whole folder, including your language folder and its campaign, multiplayer and patch `.ff` files. |

Keep the filenames and folder structure intact. Use `localization.txt` and the language assets from the same installation. Do not extract the `.iwd` or `.ff` files.

The destination is **COD4iOS's Documents folder**. In the iPhone Files app, this is **Browse → On My iPhone → COD4iOS**. It should look like this; `english` is only an example of a language folder:

```text
COD4iOS/
├── localization.txt
├── main/
│   ├── iw_00.iwd
│   ├── ...the remaining .iwd archives...
│   └── video/
│       ├── intro_movie.bik
│       ├── intro_movie.mp4       # after conversion
│       └── ...the remaining movies...
└── zone/
    └── english/
        ├── code_post_gfx.ff
        ├── ...campaign and multiplayer .ff files...
        └── cod4x_patchv2.ff      # installed by COD4iOS
```

Put `main`, `zone` and `localization.txt` directly there. An extra `Call of Duty 4/` or `Documents/` folder inside COD4iOS will prevent the app from finding them. PC executables, DLLs, Steam files and your old PC player profile are not needed.

### From a Mac

1. Close COD4iOS on the phone and keep it connected by USB.
2. Open Finder, select the iPhone in the sidebar and open **Files**.
3. Drag `localization.txt`, `main` and `zone` onto **COD4iOS**.
4. Wait for the transfer to finish, then open the app again.

Apple documents this in its [Finder file-sharing guide](https://support.apple.com/en-gb/119585).

### From Windows

Use **Apple Devices → your iPhone → Files → COD4iOS** to transfer files. See [Apple's Windows file-sharing guide](https://support.apple.com/guide/devices-windows/transfer-files-between-your-devices-mchl4bd77d3a/windows).

If your transfer tool does not accept folders, ZIP `main`, `zone` and `localization.txt` together, transfer that ZIP to COD4iOS and extract it in the iPhone Files app. Move the three items out of the extracted folder into COD4iOS's root. Check the structure above before reopening the app. ZIP transfer needs extra free space while extracting; remove the ZIP after checking the copied files.

## Enable the cutscenes

The original movies are Bink (`.bik`). COD4iOS plays converted H.264/AAC `.mp4` files instead. Without the converted files, movies are skipped.

On a Mac with `ffmpeg` installed, run this from the source folder, replacing the example path with your PC game folder:

```sh
bash ports/ios/scripts/convert_videos.sh "/path/to/Call of Duty 4" all
```

The script creates the MP4 files beside the originals in `main/video/`. Copy that updated folder to the phone. Keep the same movie names: `intro_movie.bik` becomes `intro_movie.mp4`. Using `all` converts the campaign movies too; running the script without `all` only converts the startup/menu set.

## Playing

The app starts in multiplayer on a fresh install. To switch modes, select **Single Player** or **Multiplayer** in the game menu, then close the app completely and reopen it. The selected engine loads on the next launch.

On the first multiplayer launch, COD4iOS downloads and verifies the public CoD4x compatibility patch. Let it finish before joining a server. You do not need to copy another player's patch, GUID or configuration.

The server browser filters out unknown endpoints and servers with recorded authentication rejections by default. Compatible non-Steam CoD4x servers are the intended multiplayer target. Servers requiring Steam or official-client hardware authentication may reject this client. Custom maps and mods also vary; a listed server is not a guarantee that every map or mod works.

### Controls

Touch controls appear when no supported controller is connected. Use the left stick to move and swipe the look area to aim. The on-screen buttons provide firing, aiming, weapon switching, reload/use, stance, grenades, melee, sprint, scoreboard and menu controls.

Connecting a supported controller hides the touch overlay; disconnecting it brings the overlay back. Xbox-style prompts follow the active button layout. These are the default bindings:

| Input | Action |
| --- | --- |
| Left / right stick | Move / look |
| RT / LT | Fire / aim down sights |
| A / B | Jump / change stance |
| X / Y | Use or reload / switch weapon |
| RB / LB | Frag / tactical grenade |
| Left stick click | Click to sprint; hold breath with scoped weapons |
| Right stick click | Melee |
| D-pad | Equipment/action slots |
| Start / Menu | Pause/menu |
| Back / Select / Share | Toggle scoreboard: press to open, press again to close |

Touch and controller aiming use aim assist and reduced sensitivity while aiming down sights. The engine frame limit is uncapped; actual FPS still depends on the device, display refresh rate and workload.

## Build from source

You need macOS, Xcode with the iPhoneOS SDK and C++23 support, CMake 3.24 or later, and Python 3. This release was built with Xcode 27.0. OpenAL Soft is included in the source tree.

If you already use Homebrew, install the command-line dependencies with:

```sh
brew install cmake python
# Optional, for cutscene conversion:
brew install ffmpeg
```

Download or clone the source, open Terminal in its root folder and configure the project:

```sh
bash tools/configure-ios.sh
open build/ios/xcode-engine/KisakCOD.xcodeproj
```

In Xcode:

1. Select the **KisakCOD-Combined** scheme and your connected iPhone as the destination.
2. Select the combined app target and open **Signing & Capabilities**.
3. Choose your own development team and automatic signing. If the bundle identifier is unavailable to your team, use a unique identifier for your build.
4. Build and run, then copy your game files as described above.

The build targets retain the KisakCOD name. The installed app is **COD4iOS**; the release bundle identifier is `com.devz.cod4ios`.

To make an unsigned Release IPA with the default release identifier:

```sh
xcodebuild -project build/ios/xcode-engine/KisakCOD.xcodeproj \
  -target KisakCOD-Combined -configuration Release -sdk iphoneos \
  -jobs 1 CODE_SIGNING_ALLOWED=NO build

python3 tools/package-public-playtest.py \
  --app build/ios/xcode-engine/ports/ios/Release-iphoneos/KisakCOD.app \
  --source-root "$PWD" \
  --output release/public
```

The output is `release/public/COD4iOS-playtest-unsigned.ipa`, with a separate source archive and build report. Sign it before installing. A simulator build cannot be installed on an iPhone. The public packager expects `com.devz.cod4ios`; for a build with your own identifier, use Xcode's signing/export workflow instead.

See [BUILDING.md](docs/BUILDING.md) for configuration options and developer checks.

## If something goes wrong

- **Game files not found:** check for extra parent folders and make sure all three required items are in COD4iOS's root.
- **Missing language or asset errors:** recopy the complete `main` and `zone` folders from the same PC installation. A multiplayer-only file set is not enough for the campaign.
- **Missing cutscenes:** convert all the movies and check that the MP4 files are in `main/video/`, with their original base names.
- **CoD4x patch download/checksum error:** check internet access and reopen the app to retry. Avoid replacing the verified patch with one from an unrelated mod pack.
- **Authorization timeout:** the server may require authentication this port cannot provide. Try a compatible server; reinstalling game files will not change its authentication requirements.
- **App will not open after previously working:** check whether its signing certificate has expired and refresh the installation.

When updating, use the same signing account and bundle identifier. Back up COD4iOS's Documents first, especially your profiles and saves. Deleting the app also deletes its local game files and progress.

Report bugs through this repository's Issues tab. Include your device, iOS version, app version, mission or server/map, and the steps that reproduce it. Console output is saved as `kisakcod.log` in Documents. Check logs for private information before sharing them.

## Credits and license

Based on [KisakCOD](https://github.com/SwagSoftware/KisakCOD), with CoD4x compatibility work and touch-control layout contributions from MC360-Recomp. Original project information is preserved in [the upstream README](docs/UPSTREAM_README.md).

Source licensing and third-party notices are in [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md). Call of Duty, its game data and artwork belong to their respective owners. COD4iOS is a community project and is not affiliated with Activision.
