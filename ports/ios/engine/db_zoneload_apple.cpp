// Fastfile content loading for 64-bit Apple builds.
//
// Zones on disk store their structures with 32-bit layouts, which
// src/database/db_load.cpp reads directly into memory. On arm64 that would
// corrupt every structure, so DB_LoadXFileInternal calls this instead. The
// OpenAssetTools-based loader (ports/ios/zoneload) reads the same data into the
// engine's zone blocks and native 64-bit structures; this file supplies the
// engine side of the work the 32-bit loader did inline:
//   - script strings become engine string ids (Load_ScriptStringList/_Custom)
//   - textures and sounds are handed to the renderer and sound driver
//   - per-asset load hooks (vertex declarations, shaders, world vertex buffers,
//     water, effect and sound references by name, surface zone handles)
//   - registration with DB_AddXAsset, including the technique set and menu extras

#include <universal/q_shared.h>
#include <database/database.h>
#include <qcommon/qcommon.h>
#include <script/scr_stringlist.h>

#include <xanim/xanim.h>
#include <xanim/xmodel.h>
#include <DynEntity/DynEntity_client.h>

#include <gfx_d3d/r_image.h>
#include <gfx_d3d/r_material.h>
#include <gfx_d3d/r_buffers.h>
#include <gfx_d3d/r_water.h>
#include <gfx_d3d/r_bsp.h>
#include <gfx_d3d/rb_uploadshaders.h>
#include <gfx_d3d/fxprimitives.h>
#include <EffectsCore/fx_system.h>

#include <sound/snd_public.h>
#include <ui/ui_shared.h>

#include "../zoneload/bridge/kisak_zone_bridge.h"

// src/database/db_registry.cpp (DB_AddXAsset itself is file-local there).
XAssetHeader DB_AddXAsset_Apple(XAssetType type, XAssetHeader header);

#include <map>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

// Per-zone bookkeeping. Shared sub-objects (techniques, passes' declarations and
// shaders, water) are processed once per zone, as the 32-bit loader did when it
// first loaded them.
struct ZoneLoadState
{
    std::vector<uint16_t> scriptStrings;
    std::unordered_set<void *> processed;
    std::unordered_map<MaterialVertexDeclaration *, MaterialVertexDeclaration *> vertexDecls;
};

std::mutex s_zoneHandlesMutex;
std::map<XZoneMemory *, void *> s_zoneHandles;

bool FirstVisit(ZoneLoadState &state, void *object)
{
    return object && state.processed.insert(object).second;
}

size_t ReadZoneData(void *context, void *buffer, size_t length)
{
    (void)context;
    // Generated loaders request zero bytes for empty arrays; the engine's
    // inflate reader asserts on a zero-size request and zlib reports Z_BUF_ERROR.
    if (length == 0)
        return 0;
    DB_LoadXFileData(static_cast<uint8_t *>(buffer), static_cast<uint32_t>(length));
    return length;
}

void ScriptStringsLoaded(void *context, const char **strings, int count)
{
    auto &state = *static_cast<ZoneLoadState *>(context);
    state.scriptStrings.assign(count > 0 ? static_cast<size_t>(count) : 0u, 0);
    for (int index = 0; index < count; ++index)
        state.scriptStrings[index] = strings[index] ? static_cast<uint16_t>(SL_GetString(strings[index], 4u)) : 0;
}

uint16_t ScriptString(void *context, uint16_t zoneIndex)
{
    const auto &state = *static_cast<ZoneLoadState *>(context);
    return zoneIndex < state.scriptStrings.size() ? state.scriptStrings[zoneIndex] : 0;
}

void LoadImageData(void *context, void *loadDef, void *imagePointer)
{
    (void)context; (void)loadDef;
    auto *image = static_cast<GfxImage *>(imagePointer);
    Load_Texture(&image->texture, image);
}

void SetSoundData(void *context, void *soundPointer)
{
    (void)context;
    auto *sound = static_cast<MssSoundCOD4 *>(soundPointer);
    SND_SetData(sound, sound->data);
}

// ---------------------------------------------------------------------------
// Per-asset load work

void ResolveSoundName(snd_alias_list_t *&sound)
{
    // Loaded as a pointer to the alias name (Load_SndAliasCustom).
    if (sound)
        sound = DB_FindXAssetHeader(ASSET_TYPE_SOUND, *reinterpret_cast<const char **>(sound)).sound;
}

void ResolveEffectName(FxEffectDefRef &ref)
{
    if (ref.name)
        Load_FxEffectDefFromName(&ref.name);
}

void FixupXModel(XModel *model)
{
    if (!model->surfs)
        return;
    for (unsigned surfIndex = 0; surfIndex < model->numsurfs; ++surfIndex)
        Load_GetCurrentZoneHandle(&model->surfs[surfIndex].zoneHandle);
}

void FixupMaterial(ZoneLoadState &state, Material *material)
{
    if (!material->textureTable)
        return;
    for (unsigned textureIndex = 0; textureIndex < material->textureCount; ++textureIndex)
    {
        MaterialTextureDef &def = material->textureTable[textureIndex];
        if (def.semantic == TS_WATER_MAP && FirstVisit(state, def.u.water))
            Load_PicmipWater(&def.u.water);
    }
}

void FixupTechniqueSet(ZoneLoadState &state, MaterialTechniqueSet *techniqueSet)
{
    for (MaterialTechnique *technique : techniqueSet->techniques)
    {
        if (!FirstVisit(state, technique))
            continue;
        for (unsigned passIndex = 0; passIndex < technique->passCount; ++passIndex)
        {
            MaterialPass &pass = technique->passArray[passIndex];
            if (pass.vertexDecl)
            {
                // Load_BuildVertexDecl may substitute the declaration; every pass
                // sharing the loaded one gets the same result.
                auto found = state.vertexDecls.find(pass.vertexDecl);
                if (found == state.vertexDecls.end())
                {
                    MaterialVertexDeclaration *const loaded = pass.vertexDecl;
                    Load_BuildVertexDecl(&pass.vertexDecl);
                    state.vertexDecls.emplace(loaded, pass.vertexDecl);
                }
                else
                {
                    pass.vertexDecl = found->second;
                }
            }
            if (FirstVisit(state, pass.vertexShader))
                Load_CreateMaterialVertexShader(&pass.vertexShader->prog.loadDef, pass.vertexShader);
            if (FirstVisit(state, pass.pixelShader))
                Load_CreateMaterialPixelShader(&pass.pixelShader->prog.loadDef, pass.pixelShader);
        }
    }
}

void FixupGfxWorld(GfxWorld *world)
{
    Load_VertexBuffer(&world->vd.worldVb, reinterpret_cast<uint8_t *>(world->vd.vertices), 44 * world->vertexCount);
    Load_VertexBuffer(&world->vld.layerVb, world->vld.data, world->vertexLayerDataSize);
}

void FixupEffect(const FxEffectDef *effect)
{
    if (!effect->elemDefs)
        return;
    const int elemCount = effect->elemDefCountLooping + effect->elemDefCountOneShot + effect->elemDefCountEmission;
    for (int elemIndex = 0; elemIndex < elemCount; ++elemIndex)
    {
        auto &elem = const_cast<FxElemDef &>(effect->elemDefs[elemIndex]);
        if (elem.elemType == FX_ELEM_TYPE_RUNNER)
        {
            if (elem.visualCount > 1)
            {
                for (unsigned visualIndex = 0; visualIndex < elem.visualCount; ++visualIndex)
                    ResolveEffectName(elem.visuals.array[visualIndex].effectDef);
            }
            else
            {
                ResolveEffectName(elem.visuals.instance.effectDef);
            }
        }
        ResolveEffectName(elem.effectOnImpact);
        ResolveEffectName(elem.effectOnDeath);
        ResolveEffectName(elem.effectEmitted);
    }
}

void FixupWeapon(ZoneLoadState &state, WeaponDef *weapon)
{
    for (snd_alias_list_t **sound : {
             &weapon->pickupSound, &weapon->pickupSoundPlayer, &weapon->ammoPickupSound, &weapon->ammoPickupSoundPlayer,
             &weapon->projectileSound, &weapon->pullbackSound, &weapon->pullbackSoundPlayer, &weapon->fireSound,
             &weapon->fireSoundPlayer, &weapon->fireLoopSound, &weapon->fireLoopSoundPlayer, &weapon->fireStopSound,
             &weapon->fireStopSoundPlayer, &weapon->fireLastSound, &weapon->fireLastSoundPlayer, &weapon->emptyFireSound,
             &weapon->emptyFireSoundPlayer, &weapon->meleeSwipeSound, &weapon->meleeSwipeSoundPlayer, &weapon->meleeHitSound,
             &weapon->meleeMissSound, &weapon->rechamberSound, &weapon->rechamberSoundPlayer, &weapon->reloadSound,
             &weapon->reloadSoundPlayer, &weapon->reloadEmptySound, &weapon->reloadEmptySoundPlayer, &weapon->reloadStartSound,
             &weapon->reloadStartSoundPlayer, &weapon->reloadEndSound, &weapon->reloadEndSoundPlayer, &weapon->detonateSound,
             &weapon->detonateSoundPlayer, &weapon->nightVisionWearSound, &weapon->nightVisionWearSoundPlayer,
             &weapon->nightVisionRemoveSound, &weapon->nightVisionRemoveSoundPlayer, &weapon->altSwitchSound,
             &weapon->altSwitchSoundPlayer, &weapon->raiseSound, &weapon->raiseSoundPlayer, &weapon->firstRaiseSound,
             &weapon->firstRaiseSoundPlayer, &weapon->putawaySound, &weapon->putawaySoundPlayer, &weapon->projExplosionSound,
             &weapon->projDudSound, &weapon->projIgnitionSound})
    {
        ResolveSoundName(*sound);
    }
    // The bounce sound table (one alias per surface type) can be shared.
    if (FirstVisit(state, weapon->bounceSound))
    {
        for (int surfaceType = 0; surfaceType < 29; ++surfaceType)
            ResolveSoundName(weapon->bounceSound[surfaceType]);
    }
}

// OAT reads DynEntityClient in its 32-bit on-disk layout (int physObjId, 12 bytes per entry), but the engine's
// DynEntityClient holds a pointer-sized physObjId (16 bytes on arm64). Rebuild each list in the engine layout
// before the clip map is registered, or every entry after the first is read at the wrong offset.
struct DiskDynEntityClient
{
    int32_t physObjId;
    uint16_t flags;
    uint16_t lightingHandle;
    int32_t health;
};
static_assert(sizeof(DiskDynEntityClient) == 12);

void FixupClipMap(clipMap_t *clipMap)
{
    if (sizeof(DynEntityClient) == sizeof(DiskDynEntityClient))
        return;

    static std::mutex s_convertedLock;
    // Converted lists must outlive the zone load; they are keyed by the loaded list they replace.
    static std::unordered_map<const void *, std::vector<DynEntityClient>> s_convertedByDiskList;
    static std::unordered_set<const DynEntityClient *> s_convertedLists;
    std::lock_guard<std::mutex> guard(s_convertedLock);

    for (int drawType = 0; drawType < 2; ++drawType)
    {
        const uint16_t count = clipMap->dynEntCount[drawType];
        const DynEntityClient *loadedList = clipMap->dynEntClientList[drawType];
        if (!count || !loadedList || s_convertedLists.count(loadedList))
            continue;

        const auto *diskEntries = reinterpret_cast<const DiskDynEntityClient *>(loadedList);
        std::vector<DynEntityClient> &storage = s_convertedByDiskList[loadedList];
        s_convertedLists.erase(storage.data());
        storage.assign(count, DynEntityClient{});
        for (uint16_t entryIndex = 0; entryIndex < count; ++entryIndex)
        {
            // physObjId is a runtime physics handle; DynEntCl_InitEntities creates the objects.
            storage[entryIndex].physObjId = 0;
            storage[entryIndex].flags = diskEntries[entryIndex].flags;
            storage[entryIndex].lightingHandle = diskEntries[entryIndex].lightingHandle;
            storage[entryIndex].health = diskEntries[entryIndex].health;
        }
        clipMap->dynEntClientList[drawType] = storage.data();
        s_convertedLists.insert(storage.data());
    }
}

void *LinkAsset(void *context, int assetType, void *asset)
{
    auto &state = *static_cast<ZoneLoadState *>(context);
    const auto type = static_cast<XAssetType>(assetType);

    switch (type)
    {
    case ASSET_TYPE_XMODEL:
        FixupXModel(static_cast<XModel *>(asset));
        break;
    case ASSET_TYPE_MATERIAL:
        FixupMaterial(state, static_cast<Material *>(asset));
        break;
    case ASSET_TYPE_TECHNIQUE_SET:
        FixupTechniqueSet(state, static_cast<MaterialTechniqueSet *>(asset));
        break;
    case ASSET_TYPE_GFXWORLD:
        FixupGfxWorld(static_cast<GfxWorld *>(asset));
        break;
    case ASSET_TYPE_FX:
        FixupEffect(static_cast<const FxEffectDef *>(asset));
        break;
    case ASSET_TYPE_WEAPON:
        FixupWeapon(state, static_cast<WeaponDef *>(asset));
        break;
    case ASSET_TYPE_CLIPMAP:
    case ASSET_TYPE_CLIPMAP_PVS:
        FixupClipMap(static_cast<clipMap_t *>(asset));
        break;
    default:
        break;
    }

    // Each build registers both clip map kinds under the one type its own code looks up, exactly as
    // Load_ClipMapAsset does: ASSET_TYPE_CLIPMAP ("col_map_sp") for single-player, ASSET_TYPE_CLIPMAP_PVS
    // ("col_map_mp") for multiplayer, which is what CM_LoadMap asks DB_FindXAssetHeader for.
    XAssetType registerType = type;
    if (type == ASSET_TYPE_CLIPMAP || type == ASSET_TYPE_CLIPMAP_PVS)
    {
#ifdef KISAK_MP
        registerType = ASSET_TYPE_CLIPMAP_PVS;
#else
        registerType = ASSET_TYPE_CLIPMAP;
#endif
    }
    XAssetHeader header(asset);
    const XAssetHeader registered = DB_AddXAsset_Apple(registerType, header);

    if (type == ASSET_TYPE_TECHNIQUE_SET)
    {
        // Load_MaterialTechniqueSetAsset
        Material_OriginalRemapTechniqueSet(registered.techniqueSet);
        Material_UploadShaders(registered.techniqueSet);
    }
    else if (type == ASSET_TYPE_MENU)
    {
        // Load_MenuAsset: the loaded menu's items point at the menu the engine kept.
        const menuDef_t *loaded = header.menu;
        for (int itemIndex = 0; itemIndex < loaded->itemCount; ++itemIndex)
            loaded->items[itemIndex]->parent = registered.menu;
    }

    return registered.data;
}

} // namespace

void DB_LoadXFileContent_Apple(XZoneMemory *zoneMem, const char *zoneName)
{
    KisakZoneBlocks blocks{};
    for (int blockIndex = 0; blockIndex < 9; ++blockIndex)
    {
        blocks.data[blockIndex] = zoneMem->blocks[blockIndex].data;
        blocks.size[blockIndex] = zoneMem->blocks[blockIndex].size;
    }

    ZoneLoadState state;
    KisakZoneHooks hooks{};
    hooks.context = &state;
    hooks.read = ReadZoneData;
    hooks.scriptStringsLoaded = ScriptStringsLoaded;
    hooks.scriptString = ScriptString;
    hooks.linkAsset = LinkAsset;
    hooks.loadImageData = LoadImageData;
    hooks.setSoundData = SetSoundData;

    char error[512] = "";
    void *const zone = KisakZone_Load(zoneName, blocks, hooks, error, sizeof(error));
    if (!zone)
        Com_Error(ERR_DROP, "Fastfile for zone '%s' could not be loaded: %s", zoneName, error);

    {
        std::lock_guard<std::mutex> lock(s_zoneHandlesMutex);
        s_zoneHandles[zoneMem] = zone;
    }

    // The 32-bit loader copied the vertex and index blocks into the locked static
    // buffers as it left them (DB_SetStreamIndex); the data is complete now.
    if (zoneMem->lockedVertexData && zoneMem->blocks[7].size)
        memcpy(zoneMem->lockedVertexData, zoneMem->blocks[7].data, zoneMem->blocks[7].size);
    if (zoneMem->lockedIndexData && zoneMem->blocks[8].size)
        memcpy(zoneMem->lockedIndexData, zoneMem->blocks[8].data, zoneMem->blocks[8].size);
}

void DB_FreeXFileContent_Apple(XZoneMemory *zoneMem)
{
    void *zone = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_zoneHandlesMutex);
        const auto found = s_zoneHandles.find(zoneMem);
        if (found == s_zoneHandles.end())
            return;
        zone = found->second;
        s_zoneHandles.erase(found);
    }
    KisakZone_Free(zone);
}
