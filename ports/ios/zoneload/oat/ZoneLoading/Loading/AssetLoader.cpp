#include "AssetLoader.h"

#include "kisak_zone_hooks_internal.h"

#include <algorithm>
#include <cassert>

AssetLoader::AssetLoader(const asset_type_t assetType, Zone& zone, ZoneInputStream& stream)
    : ContentLoaderBase(zone, stream),
      varScriptString(nullptr),
      m_asset_type(assetType)
{
}

XAssetInfoGeneric* AssetLoader::LinkAsset(std::string name,
                                          void* asset,
                                          std::vector<XAssetInfoGeneric*> dependencies,
                                          std::vector<scr_string_t> scriptStrings,
                                          std::vector<IndirectAssetReference> indirectAssetReferences) const
{
    auto* info = m_zone.m_pools.AddAsset(m_asset_type, std::move(name), asset, std::move(dependencies), std::move(scriptStrings), std::move(indirectAssetReferences));

    // KisakCOD: the engine runs its per-asset load work and registers the asset;
    // generated loaders then continue with the asset the engine chose.
    if (const auto* hooks = KisakZone_CurrentHooks(); hooks && info)
        info->m_ptr = hooks->linkAsset(hooks->context, static_cast<int>(m_asset_type), info->m_ptr);

    return info;
}

XAssetInfoGeneric* AssetLoader::GetAssetInfo(const std::string& name) const
{
    return m_zone.m_pools.GetAsset(m_asset_type, name);
}
