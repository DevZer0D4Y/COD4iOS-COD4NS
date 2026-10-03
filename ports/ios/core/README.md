# Portable upstream core

`kisakcod_core` compiles a growing subset of the original engine as C++17 on
macOS and iOS ARM64. It is a small engine portability milestone, not the full
game engine. The native app can call `KisakCore_RunSelfTests()` through
`include/kisak_core.h`; zero means success.

Upstream modules currently in the library:

| Module | What it provides |
| --- | --- |
| `src/qcommon/md4.cpp` | Legacy game-content checksums. |
| `src/qcommon/huffman.cpp` | Multiplayer message compression. |
| `src/universal/com_math.cpp` | Vector, matrix, quaternion and angle math. |
| `src/universal/com_math_anglevectors.cpp` | `AngleVectors`, `AnglesToAxis`, `AnglesToQuat`. |
| `src/qcommon/com_pack.cpp` | Packed vertex normals, texture coordinates and colours. |
| `src/universal/base64.cpp` | Base64 used by the engine's text encoding. |

The multiplayer Huffman frequency table is read from the original
`src/qcommon/msg_mp.cpp` by CMake, without compiling the Windows multiplayer
implementation. No Windows runtime, DirectX or proprietary game assets are
needed for this target.

`core_services.cpp` supplies `va()` and `track_static_alloc_internal()`, which
these modules call but which still live in Windows-only translation units. The
complete engine keeps its own once `q_shared.cpp` and `mem_track.cpp` are
ported. `src/universal/surfaceflags.cpp` is not included for that reason: it
needs `I_stricmp` from `q_shared.cpp`, which includes `qcommon/threads.h` and
therefore `Windows.h`.

Changes to upstream code:

- Narrow shared-header dependencies to standard headers and the assertion API.
- Use pointer-sized Huffman sort elements and typed node access on 64-bit hosts.
- Avoid writing internal-node symbol 257 beyond the 257-element lookup array.
- Read/write MD4 words explicitly in little-endian order, without unaligned
  or strict-aliasing-invalid integer pointer casts.
- Accept empty MD4 updates safely, avoid overflow in the block loop, and encode
  keyed game checksums with the Windows-compatible little-endian key order.
- Include `<climits>` in `q_shared.h`, which MSVC supplied transitively.
- State the pointer size in the `StringTable` and `SpawnVar` size assertions.
  Both describe native runtime structs that widen on 64-bit targets; the
  serialized fastfile layout is a separate problem and is still 32-bit.
- Define `__forceinline` for clang and GCC.
- Give the engine's float `random()` the distinct name `Com_RandomFloat` on
  non-Windows targets, where POSIX already declares `long random(void)` and C++
  cannot overload on the return type. Windows keeps the original symbol, so its
  call sites are unchanged.
- Guard the x86 intrinsic headers in `qcommon.h` and implement `SnapFloatToInt`
  with `lrintf()` elsewhere. `cvtss2si` rounds by MXCSR, whose default is
  round-to-nearest-ties-to-even, and `lrintf()` under the default `FE_TONEAREST`
  applies the same rule, so ARM64 rounds ties identically.
- Address the perspective-matrix builders and the quaternion/axis converters in
  `com_math.cpp` by row and column. The decompiled code used flat indices that
  ran past the first row of a `float[3]` or `float[4]` parameter.
- Replace the type punning in `com_pack.cpp`: vertex colours were written as one
  `uint32_t` through a `uint8_t *`, which is misaligned for three of every four
  vertex colours and assumed a little-endian host, and the packed texture
  coordinate helpers shifted a 16-bit half into an `int`'s sign bit.
- Guard the MSVC-only `malloc.h`, `_alloca` and `vadefs.h` uses in the vendored
  ODE headers under `deps/ode`, which `com_math.cpp` reaches through
  `xanim/dobj.h`. No ODE source is compiled for this target.

The original Huffman encoder still has a global bit cursor and an unchecked
output-buffer interface. The app's self-test serializes calls and allocates
enough space. Engine callers must size buffers and serialize calls themselves.
The original weight-only `qsort` tie behavior is preserved; cross-platform
multiplayer wire compatibility has not been verified and is not claimed.

Build and test this subset without generating the app:

```sh
cmake -S ports/ios/core -B work/build-core -DBUILD_TESTING=ON -DKISAK_CORE_SANITIZERS=ON
cmake --build work/build-core
ctest --test-dir work/build-core --output-on-failure
```

Tests exercise all seven RFC 1320 MD4 vectors, incremental block boundaries,
unaligned input/output, the million-`a` vector, game checksum wrappers, all 256
Huffman byte values, arbitrary bit offsets, deterministic binary payloads,
tree integrity, output canaries and every truncation position within symbols.
The math tests check the `AngleVectors` basis for orthonormality and COD4's
sign conventions, agreement between `AnglesToAxis`, `AnglesToQuat`/`QuatToAxis`
and `AxisToAngles`, which cells the perspective builders write, and that
`SnapFloatToInt` rounds every tie to even. The packed-vertex tests round-trip
unit normals, texture coordinates and colours, and run the colour permutations
at all four byte alignments. Base64 is checked against the RFC 4648 vectors and
round-tripped at each padding alignment. The app startup check is shorter and
verifies MD4 `abc` plus an all-byte Huffman round trip. MD4 is retained only as
a legacy game checksum.

Validation on 2026-09-09 used AppleClang 21.0.0.21000101 and Xcode's iOS 26.5
SDKs. The macOS ARM64 Debug executable passed 37,128 checks with AddressSanitizer
and UndefinedBehaviorSanitizer enabled, with `-fno-sanitize-recover=all` so that
undefined behaviour aborts instead of only printing. Release static libraries
also compiled successfully for both `iphoneos/arm64` and
`iphonesimulator/arm64`, targeting iOS 16.0. The app was run on an iPhone 17 Pro
simulator with iOS 26.5: it launched, its startup MD4/Huffman self-test passed,
and a synthetic IBSP 22 map opened, drew and framed itself from the map's
`info_player_start` angles. None of this establishes gameplay or execution on a
physical device.
