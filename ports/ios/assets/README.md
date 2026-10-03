# COD4 map geometry reader

This standalone C++17 library reads **loose little-endian IBSP version 22** maps
and world geometry from **PC `IWffu100` version 5 fastfiles** into a bounded
triangle mesh. It does not run the COD4 engine, campaign scripts, collision,
animations, sound or mission progression. Materials, textures, lightmaps and
models are not rendered. No game map is distributed with the source.

`FastfileWorld.hpp` exposes `loadMap(path)` for both formats and
`loadFastfileWorld(bytes, mapBaseName)` for PC fastfiles. The latter decompresses
zlib data, validates a unique serialized GfxWorld record and walks the supported
serialized fields to extract surfaces, vertices and a camera spawn hint. This is
not the engine asset database: it does not reconstruct the full asset graph or
resolve its native references. Inline technique sets and unsupported layouts
are rejected. Keep each fastfile's original map basename.

On 2026-09-09 the geometry reader passed on all 43 map fastfiles in the user's
installed copy, under AddressSanitizer and UndefinedBehaviorSanitizer. That is
geometry compatibility evidence, not a successful campaign run. The CLI
`kisakcod_map_inspect path/to/map.ff` reproduces an individual geometry check.

The implementation follows these upstream sources in this repository:

- `src/qcommon/com_bsp.h`: lump IDs and the version 22 header.
- `src/qcommon/com_bsp_load_obj.cpp`, `Com_GetBspLump`: sequential chunk layout.
- `src/gfx_d3d/r_bsp.h`: `DiskGfxVertex` and `DiskTriangleSoup` disk records.
- `src/gfx_d3d/r_bsp_load_obj.cpp`, `R_LoadSurfaces` and
  `R_ChooseTrisContextType`: vertex decoding and representation selection.
- `R_FinalizeSurfVerts`: indices address `firstVertex + relativeIndex`.

Version 22 has the bytes `IBSP`, a little-endian uint32 version, a uint32 chunk
count, then `{ uint32 type, uint32 byteLength }` directory entries. Payloads follow
in directory order, each aligned to four bytes. The directory is **not** an array
of offsets. Vertices are 68 bytes (XYZ at byte 0, float normal at byte 12, BGRA
color at byte 24). Surfaces are 24 bytes (`firstVertex` at 12, `vertexCount` at 16,
`indexCount` at 18, `firstIndex` at 20). The index lump contains uint16 values
relative to each surface's `firstVertex`.

When both representations exist, the unlayered one is selected. They are
alternative representations of the same world, so they must not be concatenated.
When the unlayered surface lump is absent or empty, the layered representation is
selected. A broken selected representation is reported as an error, not hidden by
silently selecting the other one. Vertex layer data is unnecessary for this
geometry-only output. Bounds include only vertices referenced by surfaces.

`BspLoader.hpp` exposes `kisakcod::assets::loadBsp(bytes)` and `loadBsp(path)`.
Coordinates remain Z-up in COD4 units, output indices are absolute uint32, normals
are normalized (zero normals become +Z), and colors are RGBA floats. Spawn
selection prefers `info_player_start`, then `info_player_deathmatch`, then an
`mp_*_spawn` entity. Spawn data is only a camera hint; no entity code runs.

The hard ceilings are 128 MiB of file bytes, 1,000,000 vertices, 6,000,000 disk and
expanded indices, 65,536 surfaces, 100 chunks and 4 MiB of entity text. Callers can
lower these through `BspLoadLimits`. The parser checks byte ranges, record sizes,
duplicate chunks, finite coordinates, local index ranges and expansion budgets.
Unknown lump payloads are range-checked but otherwise ignored. Errors throw
`BspError`. Loading very large maps should run off the UI thread.

The fastfile reader requires zlib. Build in a parent CMake project with `add_subdirectory(assets)` and
link `kisakcod_assets`, or run its host-only tests independently:

```sh
cmake -S ports/ios/assets -B work/bsp-tests -DBUILD_TESTING=ON
cmake --build work/bsp-tests
ctest --test-dir work/bsp-tests --output-on-failure
```

Tests construct small format fixtures in memory, including nonzero base vertices,
out-of-order lumps and both representations. They check every truncation of a
valid fixture, invalid versions/ranges/floats/entities, index-expansion budgets,
file loading and 2,000 deterministic byte mutations. These fixtures are parser
tests, not game content, and do not establish compatibility with a retail map.
