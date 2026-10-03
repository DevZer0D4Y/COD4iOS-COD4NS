# Changelog

## 1.0.3 (build 4) — 3 October 2026

- Make multiplayer browser filtering work with empty Documents using 26 independently verified public server endpoints.
- Cache explicit authentication rejection and let recent local outcomes override the bundled baseline. Unknown servers are hidden by default; a successful direct join can add one locally.
- Default new iOS multiplayer profiles to `cl_maxpackets 125` and `snaps 40`, retaining `rate 25000` and `cl_packetdup 1`.
- Include the current campaign and multiplayer engines with all earlier port corrections, including the CoD4x wire/Huffman transport, mod/custom-map download handling, spawn/camera/HUD work, touch/controller input, aim assist, sprint/scoreboard toggles, audio cleanup, save handling and frame-rate changes.

## 1.0.2 (build 3) — 3 October 2026

- Bootstrap the public `cod4x_patchv2.ff` dependency from a pinned official revision, verify SHA-256, preserve an existing mismatched patch and recover other language copies locally.
- Send the same persistent per-install GUID in the non-Steam challenge and connection packets; generate new Apple identities with system randomness.

These notes describe included changes, not universal gameplay certification. Steam/official-client authentication is not implemented. Gameplay on every campaign mission and every server has not been verified.
