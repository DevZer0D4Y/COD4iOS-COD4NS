#include "gfximage_actions.h"

#include "kisak_zone_hooks_internal.h"

#include <cassert>
#include <cstring>

using namespace IW3;

Actions_GfxImage::Actions_GfxImage(Zone& zone)
    : AssetLoadingActions(zone)
{
}

void Actions_GfxImage::OnImageLoaded(GfxImage* image) const
{
    // KisakCOD: Load_Texture already filled in the card memory; keep it.
    if (KisakZone_CurrentHooks())
        return;
    image->cardMemory.platform[0] = 0;
}

void Actions_GfxImage::LoadImageData(GfxImageLoadDef* loadDef, GfxImage* image) const
{
    const size_t loadDefSize = offsetof(GfxImageLoadDef, data) + loadDef->resourceSize;

    image->texture.loadDef = static_cast<GfxImageLoadDef*>(m_zone.Memory().AllocRaw(loadDefSize));
    memcpy(image->texture.loadDef, loadDef, loadDefSize);

    // KisakCOD: create the texture from the persistent copy (Load_Texture).
    if (const auto* hooks = KisakZone_CurrentHooks())
        hooks->loadImageData(hooks->context, image->texture.loadDef, image);
}
