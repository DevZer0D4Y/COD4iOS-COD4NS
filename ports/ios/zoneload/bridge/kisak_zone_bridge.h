#pragma once

// Boundary between KisakCOD and the OpenAssetTools zone loader. Deliberately free
// of engine and OAT types: each side includes only this header.

#include <cstddef>
#include <cstdint>

struct KisakZoneBlocks
{
    // The engine's XZoneMemory blocks, in XFILE_BLOCK_* order. The loader reads
    // into this memory so block-relative addresses match what the engine expects.
    uint8_t *data[9];
    uint32_t size[9];
};

struct KisakZoneHooks
{
    void *context;

    // Decompressed zone data following the XFile header.
    size_t (*read)(void *context, void *buffer, size_t length);

    // The zone's script string list has been read; map its entries to engine string ids.
    void (*scriptStringsLoaded)(void *context, const char **strings, int count);
    // Engine string id for a zone-local script string index.
    uint16_t (*scriptString)(void *context, uint16_t zoneIndex);

    // An asset finished loading. Performs the engine's per-asset load work and
    // registers it; returns the asset the engine will use (an existing one may win).
    void *(*linkAsset)(void *context, int assetType, void *asset);

    // GfxImage texture data is available (Load_Texture).
    void (*loadImageData)(void *context, void *loadDef, void *image);
    // LoadedSound sample data is available (SND_SetData).
    void (*setSoundData)(void *context, void *sound);
};

// Loads the XAssetList of an IW3 PC zone into native 64-bit structures. Returns a
// handle owning the zone's out-of-block allocations (release it with KisakZone_Free
// when the engine frees the zone's memory), or null with a message in errorOut.
void *KisakZone_Load(const char *zoneName, const KisakZoneBlocks &blocks, const KisakZoneHooks &hooks, char *errorOut, size_t errorSize);
void KisakZone_Free(void *zone);
