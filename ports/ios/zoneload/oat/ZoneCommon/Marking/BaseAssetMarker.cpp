#include "BaseAssetMarker.h"

#include <algorithm>
#include <cassert>
#include <unordered_map>

namespace
{
// KisakCOD: script strings are converted in place to engine ids. Zones share identical arrays (offset pointers to data already
// loaded, e.g. weapon model boneNames), so a slot must only be converted once per zone load or its ids get remapped twice.
// Maps a slot to the engine id written there; temp-block memory is reused for new data, so a slot only counts as
// converted while it still holds that id.
thread_local std::unordered_map<const scr_string_t *, scr_string_t> s_convertedScriptStrings;
} // namespace

void KisakZone_ResetConvertedScriptStrings()
{
    s_convertedScriptStrings.clear();
}

BaseAssetMarker::BaseAssetMarker(AssetVisitor& visitor)
    : m_visitor(visitor)
{
}

void BaseAssetMarker::Mark_ScriptString(scr_string_t& scriptString) const
{
    const auto result = m_visitor.Visit_ScriptString(scriptString);
    if (result.has_value())
        scriptString = *result;
}

void BaseAssetMarker::MarkArray_ScriptString(scr_string_t* scriptStringArray, const size_t count) const
{
    assert(scriptStringArray != nullptr);

    for (size_t index = 0; index < count; index++)
    {
        // Arrays can be shared by several assets in one zone (offset pointers), so convert each slot only once.
        scr_string_t &slot = scriptStringArray[index];
        const auto converted = s_convertedScriptStrings.find(&slot);
        if (converted != s_convertedScriptStrings.end() && converted->second == slot)
            continue;
        Mark_ScriptString(slot);
        s_convertedScriptStrings[&slot] = slot;
    }
}

void BaseAssetMarker::Mark_IndirectAssetRef(const asset_type_t assetType, const char* assetName) const
{
    if (!assetName || !assetName[0])
        return;

    m_visitor.Visit_IndirectAssetRef(assetType, assetName);
}

void BaseAssetMarker::MarkArray_IndirectAssetRef(const asset_type_t assetType, const char** assetNames, const size_t count) const
{
    assert(assetNames != nullptr);

    for (size_t index = 0; index < count; index++)
        Mark_IndirectAssetRef(assetType, assetNames[index]);
}
