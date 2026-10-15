#ifndef KISAK_SP 
#error This file is for SinglePlayer only 
#endif

#include <universal/q_shared.h>
#include <qcommon/qcommon.h>
#include "mission.h"
#if defined(__APPLE__) && !defined(KISAK_NATIVE_PORT)
#include "../../ports/ios/save/CampaignProgress.h"
#include <qcommon/com_playerprofile.h>
#include <universal/com_files.h>
#include <client/client.h>
#include <game/savedevice.h>
#endif

const dvar_t *mis_cheat;
const dvar_t *mis_01;
const dvar_t *mis_difficulty;

void Campaign_UnlockAll()
{
	Dvar_SetBool(mis_cheat, 1);
}

void Campaign_RegisterDvars()
{
#if defined(__APPLE__) && !defined(KISAK_NATIVE_PORT)
    constexpr unsigned flags=DVAR_ARCHIVE;
#else
    constexpr unsigned flags=0;
#endif
    mis_01 = Dvar_RegisterInt("mis_01", 0, 0, 50, flags, "Mission 1");
    mis_difficulty = Dvar_RegisterString(
        "mis_difficulty",
        "00000000000000000000000000000000000000000000000000",
        flags,
        "Mission difficulty");
    mis_cheat = Dvar_RegisterBool("mis_cheat", 0, 0, "Set when level unlock cheat is performed");
#if defined(__APPLE__) && !defined(KISAK_NATIVE_PORT)
    // Startup configs choose the active profile before SV/CL register these
    // dvars. Recover again now that the actual typed variables exist.
    Campaign_RestoreProgress();
    SaveDevice_RestoreResume();
#endif
}

void Campaign_SaveProgress()
{
#if defined(__APPLE__) && !defined(KISAK_NATIVE_PORT)
    if(!mis_01 || !mis_difficulty || !com_playerProfile || !com_playerProfile->current.string[0])return;
    char players[260];FS_BuildOSPath(fs_homepath->current.string,"players","",players);
    kisak::campaign::Progress value{static_cast<unsigned>(mis_01->current.integer),mis_difficulty->current.string};
    if(!kisak::campaign::Save(players,com_playerProfile->current.string,value))
        Com_PrintWarning(CON_CHANNEL_FILES,"Campaign progress could not be saved; previous progress retained.\n");
#endif
}

void Campaign_BeginProfile()
{
#if defined(__APPLE__) && !defined(KISAK_NATIVE_PORT)
    if(mis_01)Dvar_SetInt(mis_01,0);
    if(mis_difficulty)Dvar_SetString(mis_difficulty,"00000000000000000000000000000000000000000000000000");
    if(mis_cheat)Dvar_SetBool(mis_cheat,0);
#endif
}

void Campaign_RestoreProgress()
{
#if defined(__APPLE__) && !defined(KISAK_NATIVE_PORT)
    static_assert(offsetof(SaveHeader,mapName)==32);
    if(!mis_01 || !mis_difficulty || !com_playerProfile || !com_playerProfile->current.string[0])return;
    char players[260];FS_BuildOSPath(fs_homepath->current.string,"players","",players);
    const std::string profile=com_playerProfile->current.string;
    kisak::campaign::Progress config{static_cast<unsigned>(mis_01->current.integer),mis_difficulty->current.string};
    auto value=kisak::campaign::Merge(config,kisak::campaign::Load(players,profile));
    value.unlocked=std::max(value.unlocked,kisak::campaign::ReachedFromSaves(players,profile));
    Dvar_SetInt(mis_01,value.unlocked);Dvar_SetString(mis_difficulty,value.difficulty.c_str());
    Campaign_SaveProgress();
#endif
}
