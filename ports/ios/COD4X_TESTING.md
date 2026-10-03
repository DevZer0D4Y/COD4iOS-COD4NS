# Multiplayer loading regression — 2026-09-21

The client reached CA_PRIMED after loading the map but stayed on “Waiting for other players”. The retail pure-file command omitted the language and full server id required by CoD4x protocol 21. The server interpreted an archive checksum as a server id and ignored the verification command, preventing the first active snapshot.

Changes:
- Send `cp @ L<language> <server-id> <referenced-IWD-checksums> <aggregate>` on CoD4x connections, including referenced localized archives and the actual checksum feed. Keep the retail command for retail servers. Reject oversized verification commands instead of truncating them.
- Skip intro cinematics for an explicit startup connection so they cannot replace the connection state.
- Reject snapshots that regress time within the same CoD4x server-count epoch and request a fresh snapshot through the existing no-delta path.
- Bound unsupported HUD alignment at rendering time while preserving wire values for future XOR deltas. Stop parsing an already truncated player/entity snapshot.
- Log the first active snapshot for runtime verification.

Validation:
- Simulator ARM64 multiplayer target built successfully using Xcode 27, one compile job.
- Live protocol 21 connection to `31.20.193.110:28100` on `mp_pipeline` passed file verification and reached an active snapshot; the simulator rendered the map, first-person weapon, minimap and HUD.
- A subsequent test exposed a regressing timestamp and invalid HUD fields; the recovery changes above were added for these cases.

This is a targeted fix for the loading regression. It does not certify all public servers, custom mods, hardware devices, or long multiplayer sessions.

## Additional server joins and controller work — 2026-09-22

Further changes:
- Preserve up to 2048 received server commands for cgame across presentation stalls. The network protocol retains its original 128-command ring. Entries carry their sequence and are invalidated at every gamestate; genuinely lost commands still fail instead of being silently skipped.
- Accept validated CoD4x usermap IWD/FF references, generate the `usermaps/<map>/<file>` download paths, mount referenced map archives after filesystem restart and resolve downloaded map fastfiles. Retail archive checksum verification remains enforced.
- Fix shader-constant insertion: the decompiled code indexed from a 32-bit count into a 16-bit register array and pointer storage, producing unstable material comparisons. Use register-indexed insertion and a strict ordering for NaN constants.
- Fix mod weapon configuration parsing. `CSPFT_XMODEL`, `CSPFT_FX`, `CSPFT_MATERIAL`, and `CSPFT_SOUND` stored pointers through `uint32_t`, truncating native 64-bit addresses. Write each complete native pointer. This directly addresses the APG loading crash in `XModelNumBones` through `CG_RegisterWeapon` (report `KisakCOD-MP-2026-09-22-080125.ips`).
- Release the Apple implicit swap chain's caller reference without expecting the device-owned reference to disappear before device shutdown.
- Connect gamepad analog movement/look to multiplayer command creation, honor configured inner/outer stick and trigger deadzones, suppress trigger chatter, separate left-stick menu focus from right-stick pointer motion, release inputs on disable/reconnect/layout changes, and resolve button glyphs through the selected layout.

Verification actually completed:
- Simulator ARM64 `KisakCOD-MP` successfully rebuilt, one compile job, final build 2026-09-22 13:41 local.
- ASan/UBSan tests pass for native config-parser pointer preservation (production parser with stub registries), 1000 shader-constant insertion permutations and strict float ordering, controller state transitions/deadzones/triggers, command-history rollover, and usermap paths.
- Live `31.20.193.110:28100`: reached active snapshots and rendered the world/HUD on `mp_convoy`; the next `mp_pipeline` gamestate also reached an active snapshot. Packet-reader assertions occurred during transitions and need further investigation.
- Live `54.36.177.240:28930` (APG), retest after unlocking on September 22: the native-pointer fix now passes weapon registration and completes game-media loading on `mp_farm`. The run still fails before verified gameplay: Device Hub displayed `BG_EvaluateTrajectory: unknown trType: 201`. This is a remaining defect, not a successful join. Added Apple stack traces to `Com_PrintStackTrace` for errors that unwind without producing a crash report; this diagnostic build compiled and launched, but UI control then failed with `Sky Computer Use native pipe startup failed`, including after a tool reset. Its last log stops during the second map initialization. No squad/class selection or spawn has been verified on APG.
- `91.210.224.49:28960`: protocol handshake accepted, authentication requested, then explicit server rejection `Authorization failed to complete within the timeout limit.` No active game. Do not describe this as a working join or as proven Steam-only rejection.
- Protocol-only probes to `188.68.60.242:27211` and `67.211.202.180:28960` accepted the handshake but supplied no gamestate within the probe window. These are not successful gameplay tests.

Pending: re-run APG to squad/class selection and an actual spawn, verify custom-map downloads end-to-end, exercise sustained online/controller gameplay on hardware, investigate malformed/truncated snapshot transitions, and determine supported authentication for the servers that reject this native client. Controller rumble and physical button-to-action testing remain unverified. This report does not certify 100% multiplayer compatibility.

Useful isolated checks (from repository root):
```
python3 ports/ios/engine/tests/ConfigParserPointerTests.py
clang++ -std=c++17 -fsanitize=address,undefined ports/ios/input/ShaderConstantTests.cpp -o /tmp/kisak-shader-tests && /tmp/kisak-shader-tests
clang++ -std=c++17 -fsanitize=address,undefined ports/ios/input/ControllerTests.cpp -o /tmp/kisak-controller-tests && /tmp/kisak-controller-tests
clang++ -std=c++17 -fsanitize=address,undefined ports/ios/network/Cod4xCompatibilityTests.cpp -o /tmp/kisak-compatibility-tests && /tmp/kisak-compatibility-tests
```
