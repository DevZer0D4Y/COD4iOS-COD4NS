#include "loadedsound_actions.h"

#include "kisak_zone_hooks_internal.h"

#include <cstring>

using namespace IW3;

Actions_LoadedSound::Actions_LoadedSound(Zone& zone)
    : AssetLoadingActions(zone)
{
}

void Actions_LoadedSound::SetSoundData(MssSound* sound) const
{
    if (sound->info.data_len > 0)
    {
        const auto* tempData = sound->data;
        sound->data = m_zone.Memory().Alloc<char>(sound->info.data_len);
        memcpy(sound->data, tempData, sound->info.data_len);
    }
    else
    {
        sound->data = nullptr;
    }

    // KisakCOD: hand the samples to the sound driver (SND_SetData).
    if (const auto* hooks = KisakZone_CurrentHooks())
        hooks->setSoundData(hooks->context, sound);
}
