# Source and third-party notices

This is an iOS port of [SwagSoftware/KisakCOD](https://github.com/SwagSoftware/KisakCOD). Keep the root GPLv3 `LICENSE`, upstream credits and the license/copyright notices in individual source files when redistributing. The source snapshot includes the current SP and MP port code; proprietary Windows SDK/runtime binaries are not included.

| Component | Included notice |
| --- | --- |
| KisakCOD | `LICENSE`, `docs/UPSTREAM_README.md`, original source headers |
| OpenAssetTools zone loader | `ports/ios/zoneload/LICENSE.OpenAssetTools` and source notices under `ports/ios/zoneload/oat` |
| DXVK-derived compatibility headers | `ports/ios/compat/native/windows/LICENSE.dxvk` |
| MinGW-w64/DirectX compatibility headers | `ports/ios/compat/native/directx/COPYING.MinGW-w64.txt` and individual header notices |
| OpenAL Soft 1.25.2 | `third-party/openal-soft/COPYING` and bundled component licenses |
| MC360-Recomp touch layout / Ben Vanik contributions | `ports/ios/app/TouchControls.LICENSE` (BSD 3-Clause) |
| ODE, zlib, Speex and other retained upstream dependencies | Notices in the corresponding `deps/` and `src/` files |

CoD4x wire compatibility references the public [CoD4x client](https://github.com/callofduty4x/CoD4x_Client_pub). The compatibility patch is downloaded at runtime from a pinned upstream revision and hash checked; its fastfile is not included in this repository or IPA. The bundled server baseline contains public endpoints independently probed with fresh test identities, not an exported player profile or history.

No Call of Duty 4 retail archives, campaign movies or player data are supplied. Names, trademarks and artwork remain the property of their respective owners. Preserve these notices alongside binary releases and supply the matching source.
