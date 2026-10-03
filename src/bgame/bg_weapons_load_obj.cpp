#include <universal/q_shared.h>
#include "bg_local.h"
#include "bg_public.h"
#include <qcommon/mem_track.h>
#include <database/database.h>
#include <universal/q_parse.h>
#include <universal/com_memory.h>
#include <universal/com_files.h>
#include <universal/com_sndalias.h>
#include <universal/surfaceflags.h>

//int surfaceTypeSoundListCount 828010f0     bg_weapons_load_obj.obj
//struct SurfaceTypeSoundList *surfaceTypeSoundLists 828011f8     bg_weapons_load_obj.obj

uint32_t g_playerAnimTypeNamesCount;

SurfaceTypeSoundList surfaceTypeSoundLists[16];

const char *stickinessNames[4] =
{
  "Don't stick",
  "Stick to all",
  "Stick to ground",
  "Stick to ground, maintain yaw"
}; // idb
const char *weapIconRatioNames[3] = { "1:1", "2:1", "4:1" }; // idb
const char *ammoCounterClipNames[7] =
{
  "None",
  "Magazine",
  "ShortMagazine",
  "Shotgun",
  "Rocket",
  "Beltfed",
  "AltWeapon"
}; // idb
const char *overlayInterfaceNames[3] = { "None", "Javelin", "Turret Scope" }; // idb
const char *szWeapFireTypeNames[5] =
{
  "Full Auto",
  "Single Shot",
  "2-Round Burst",
  "3-Round Burst",
  "4-Round Burst"
}; // idb
const char *szWeapInventoryTypeNames[4] = { "primary", "offhand", "item", "altmode" }; // idb
const char *penetrateTypeNames[4] = { "none", "small", "medium", "large" }; // idb
const char *szWeapOverlayReticleNames[2] = { "none", "crosshair" }; // idb
const char *szWeapStanceNames[3] = { "stand", "duck", "prone" }; // idb
const char *accuracyDirName[3] = { "aivsai", "aivsplayer", NULL }; // idb
const char *activeReticleNames[3] = { "None", "Pip-On-A-Stick", "Bouncing diamond" }; // idb
const char *szWeapTypeNames[4] = { "bullet", "grenade", "projectile", "binoculars" }; // idb
const char *guidedMissileNames[4] = { "None", "Sidewinder", "Hellfire", "Javelin" }; // idb
const char *offhandClassNames[4] = { "None", "Frag Grenade", "Smoke Grenade", "Flash Grenade" }; // idb
const char *szProjectileExplosionNames[7] = { "grenade", "rocket", "flashbang", "none", "dud", "smoke", "heavy explosive" }; // idb

const char *impactTypeNames[9] =
{
  "none",
  "bullet_small",
  "bullet_large",
  "bullet_ap",
  "shotgun",
  "grenade_bounce",
  "grenade_explode",
  "rocket_explode",
  "projectile_dud"
}; // idb

// These were byte offsets into the 32-bit WeaponDef. Pointers double in width on 64-bit, so the
// parser stored every value in the wrong field (and then crashed following one). offsetof keeps
// them right on both; the field names were recovered from the original offsets.
cspField_t weaponDefFields[502] =
{
  { "displayName", (int)offsetof(WeaponDef, szDisplayName), CSPFT_STRING },
  { "AIOverlayDescription", (int)offsetof(WeaponDef, szOverlayName), CSPFT_STRING },
  { "modeName", (int)offsetof(WeaponDef, szModeName), CSPFT_STRING },
  { "playerAnimType", (int)offsetof(WeaponDef, playerAnimType), WFT_ANIMTYPE },
  { "gunModel", (int)offsetof(WeaponDef, gunXModel[0]), CSPFT_XMODEL },
  { "gunModel2", (int)offsetof(WeaponDef, gunXModel[1]), CSPFT_XMODEL },
  { "gunModel3", (int)offsetof(WeaponDef, gunXModel[2]), CSPFT_XMODEL },
  { "gunModel4", (int)offsetof(WeaponDef, gunXModel[3]), CSPFT_XMODEL },
  { "gunModel5", (int)offsetof(WeaponDef, gunXModel[4]), CSPFT_XMODEL },
  { "gunModel6", (int)offsetof(WeaponDef, gunXModel[5]), CSPFT_XMODEL },
  { "gunModel7", (int)offsetof(WeaponDef, gunXModel[6]), CSPFT_XMODEL },
  { "gunModel8", (int)offsetof(WeaponDef, gunXModel[7]), CSPFT_XMODEL },
  { "gunModel9", (int)offsetof(WeaponDef, gunXModel[8]), CSPFT_XMODEL },
  { "gunModel10", (int)offsetof(WeaponDef, gunXModel[9]), CSPFT_XMODEL },
  { "gunModel11", (int)offsetof(WeaponDef, gunXModel[10]), CSPFT_XMODEL },
  { "gunModel12", (int)offsetof(WeaponDef, gunXModel[11]), CSPFT_XMODEL },
  { "gunModel13", (int)offsetof(WeaponDef, gunXModel[12]), CSPFT_XMODEL },
  { "gunModel14", (int)offsetof(WeaponDef, gunXModel[13]), CSPFT_XMODEL },
  { "gunModel15", (int)offsetof(WeaponDef, gunXModel[14]), CSPFT_XMODEL },
  { "gunModel16", (int)offsetof(WeaponDef, gunXModel[15]), CSPFT_XMODEL },
  { "handModel", (int)offsetof(WeaponDef, handXModel), CSPFT_XMODEL },
  { "hideTags", (int)offsetof(WeaponDef, hideTags[0]), WFT_HIDETAGS },
  { "notetrackSoundMap", (int)offsetof(WeaponDef, notetrackSoundMapKeys[0]), WFT_NOTETRACKSOUNDMAP },
  { "idleAnim", (int)offsetof(WeaponDef, szXAnims[1]), CSPFT_STRING },
  { "emptyIdleAnim", (int)offsetof(WeaponDef, szXAnims[2]), CSPFT_STRING },
  { "fireAnim", (int)offsetof(WeaponDef, szXAnims[3]), CSPFT_STRING },
  { "holdFireAnim", (int)offsetof(WeaponDef, szXAnims[4]), CSPFT_STRING },
  { "lastShotAnim", (int)offsetof(WeaponDef, szXAnims[5]), CSPFT_STRING },
  { "detonateAnim", (int)offsetof(WeaponDef, szXAnims[25]), CSPFT_STRING },
  { "rechamberAnim", (int)offsetof(WeaponDef, szXAnims[6]), CSPFT_STRING },
  { "meleeAnim", (int)offsetof(WeaponDef, szXAnims[7]), CSPFT_STRING },
  { "meleeChargeAnim", (int)offsetof(WeaponDef, szXAnims[8]), CSPFT_STRING },
  { "reloadAnim", (int)offsetof(WeaponDef, szXAnims[9]), CSPFT_STRING },
  { "reloadEmptyAnim", (int)offsetof(WeaponDef, szXAnims[10]), CSPFT_STRING },
  { "reloadStartAnim", (int)offsetof(WeaponDef, szXAnims[11]), CSPFT_STRING },
  { "reloadEndAnim", (int)offsetof(WeaponDef, szXAnims[12]), CSPFT_STRING },
  { "raiseAnim", (int)offsetof(WeaponDef, szXAnims[13]), CSPFT_STRING },
  { "dropAnim", (int)offsetof(WeaponDef, szXAnims[15]), CSPFT_STRING },
  { "firstRaiseAnim", (int)offsetof(WeaponDef, szXAnims[14]), CSPFT_STRING },
  { "altRaiseAnim", (int)offsetof(WeaponDef, szXAnims[16]), CSPFT_STRING },
  { "altDropAnim", (int)offsetof(WeaponDef, szXAnims[17]), CSPFT_STRING },
  { "quickRaiseAnim", (int)offsetof(WeaponDef, szXAnims[18]), CSPFT_STRING },
  { "quickDropAnim", (int)offsetof(WeaponDef, szXAnims[19]), CSPFT_STRING },
  { "emptyRaiseAnim", (int)offsetof(WeaponDef, szXAnims[20]), CSPFT_STRING },
  { "emptyDropAnim", (int)offsetof(WeaponDef, szXAnims[21]), CSPFT_STRING },
  { "sprintInAnim", (int)offsetof(WeaponDef, szXAnims[22]), CSPFT_STRING },
  { "sprintLoopAnim", (int)offsetof(WeaponDef, szXAnims[23]), CSPFT_STRING },
  { "sprintOutAnim", (int)offsetof(WeaponDef, szXAnims[24]), CSPFT_STRING },
  { "nightVisionWearAnim", (int)offsetof(WeaponDef, szXAnims[26]), CSPFT_STRING },
  { "nightVisionRemoveAnim", (int)offsetof(WeaponDef, szXAnims[27]), CSPFT_STRING },
  { "adsFireAnim", (int)offsetof(WeaponDef, szXAnims[28]), CSPFT_STRING },
  { "adsLastShotAnim", (int)offsetof(WeaponDef, szXAnims[29]), CSPFT_STRING },
  { "adsRechamberAnim", (int)offsetof(WeaponDef, szXAnims[30]), CSPFT_STRING },
  { "adsUpAnim", (int)offsetof(WeaponDef, szXAnims[31]), CSPFT_STRING },
  { "adsDownAnim", (int)offsetof(WeaponDef, szXAnims[32]), CSPFT_STRING },
  { "script", (int)offsetof(WeaponDef, szScript), CSPFT_STRING },
  { "weaponType", (int)offsetof(WeaponDef, weapType), WFT_WEAPONTYPE },
  { "weaponClass", (int)offsetof(WeaponDef, weapClass), WFT_WEAPONCLASS },
  { "penetrateType", (int)offsetof(WeaponDef, penetrateType), WFT_PENETRATE_TYPE },
  { "impactType", (int)offsetof(WeaponDef, impactType), WFT_IMPACT_TYPE },
  { "inventoryType", (int)offsetof(WeaponDef, inventoryType), WFT_INVENTORYTYPE },
  { "fireType", (int)offsetof(WeaponDef, fireType), WFT_FIRETYPE },
  { "offhandClass", (int)offsetof(WeaponDef, offhandClass), WFT_OFFHAND_CLASS },
  { "viewFlashEffect", (int)offsetof(WeaponDef, viewFlashEffect), CSPFT_FX },
  { "worldFlashEffect", (int)offsetof(WeaponDef, worldFlashEffect), CSPFT_FX },
  { "pickupSound", (int)offsetof(WeaponDef, pickupSound), CSPFT_SOUND },
  { "pickupSoundPlayer", (int)offsetof(WeaponDef, pickupSoundPlayer), CSPFT_SOUND },
  { "ammoPickupSound", (int)offsetof(WeaponDef, ammoPickupSound), CSPFT_SOUND },
  { "ammoPickupSoundPlayer", (int)offsetof(WeaponDef, ammoPickupSoundPlayer), CSPFT_SOUND },
  { "projectileSound", (int)offsetof(WeaponDef, projectileSound), CSPFT_SOUND },
  { "pullbackSound", (int)offsetof(WeaponDef, pullbackSound), CSPFT_SOUND },
  { "pullbackSoundPlayer", (int)offsetof(WeaponDef, pullbackSoundPlayer), CSPFT_SOUND },
  { "fireSound", (int)offsetof(WeaponDef, fireSound), CSPFT_SOUND },
  { "fireSoundPlayer", (int)offsetof(WeaponDef, fireSoundPlayer), CSPFT_SOUND },
  { "loopFireSound", (int)offsetof(WeaponDef, fireLoopSound), CSPFT_SOUND },
  { "loopFireSoundPlayer", (int)offsetof(WeaponDef, fireLoopSoundPlayer), CSPFT_SOUND },
  { "stopFireSound", (int)offsetof(WeaponDef, fireStopSound), CSPFT_SOUND },
  { "stopFireSoundPlayer", (int)offsetof(WeaponDef, fireStopSoundPlayer), CSPFT_SOUND },
  { "lastShotSound", (int)offsetof(WeaponDef, fireLastSound), CSPFT_SOUND },
  { "lastShotSoundPlayer", (int)offsetof(WeaponDef, fireLastSoundPlayer), CSPFT_SOUND },
  { "emptyFireSound", (int)offsetof(WeaponDef, emptyFireSound), CSPFT_SOUND },
  { "emptyFireSoundPlayer", (int)offsetof(WeaponDef, emptyFireSoundPlayer), CSPFT_SOUND },
  { "meleeSwipeSound", (int)offsetof(WeaponDef, meleeSwipeSound), CSPFT_SOUND },
  { "meleeSwipeSoundPlayer", (int)offsetof(WeaponDef, meleeSwipeSoundPlayer), CSPFT_SOUND },
  { "meleeHitSound", (int)offsetof(WeaponDef, meleeHitSound), CSPFT_SOUND },
  { "meleeMissSound", (int)offsetof(WeaponDef, meleeMissSound), CSPFT_SOUND },
  { "rechamberSound", (int)offsetof(WeaponDef, rechamberSound), CSPFT_SOUND },
  { "rechamberSoundPlayer", (int)offsetof(WeaponDef, rechamberSoundPlayer), CSPFT_SOUND },
  { "reloadSound", (int)offsetof(WeaponDef, reloadSound), CSPFT_SOUND },
  { "reloadSoundPlayer", (int)offsetof(WeaponDef, reloadSoundPlayer), CSPFT_SOUND },
  { "reloadEmptySound", (int)offsetof(WeaponDef, reloadEmptySound), CSPFT_SOUND },
  { "reloadEmptySoundPlayer", (int)offsetof(WeaponDef, reloadEmptySoundPlayer), CSPFT_SOUND },
  { "reloadStartSound", (int)offsetof(WeaponDef, reloadStartSound), CSPFT_SOUND },
  { "reloadStartSoundPlayer", (int)offsetof(WeaponDef, reloadStartSoundPlayer), CSPFT_SOUND },
  { "reloadEndSound", (int)offsetof(WeaponDef, reloadEndSound), CSPFT_SOUND },
  { "reloadEndSoundPlayer", (int)offsetof(WeaponDef, reloadEndSoundPlayer), CSPFT_SOUND },
  { "detonateSound", (int)offsetof(WeaponDef, detonateSound), CSPFT_SOUND },
  { "detonateSoundPlayer", (int)offsetof(WeaponDef, detonateSoundPlayer), CSPFT_SOUND },
  { "nightVisionWearSound", (int)offsetof(WeaponDef, nightVisionWearSound), CSPFT_SOUND },
  { "nightVisionWearSoundPlayer", (int)offsetof(WeaponDef, nightVisionWearSoundPlayer), CSPFT_SOUND },
  { "nightVisionRemoveSound", (int)offsetof(WeaponDef, nightVisionRemoveSound), CSPFT_SOUND },
  { "nightVisionRemoveSoundPlayer", (int)offsetof(WeaponDef, nightVisionRemoveSoundPlayer), CSPFT_SOUND },
  { "raiseSound", (int)offsetof(WeaponDef, raiseSound), CSPFT_SOUND },
  { "raiseSoundPlayer", (int)offsetof(WeaponDef, raiseSoundPlayer), CSPFT_SOUND },
  { "firstRaiseSound", (int)offsetof(WeaponDef, firstRaiseSound), CSPFT_SOUND },
  { "firstRaiseSoundPlayer", (int)offsetof(WeaponDef, firstRaiseSoundPlayer), CSPFT_SOUND },
  { "altSwitchSound", (int)offsetof(WeaponDef, altSwitchSound), CSPFT_SOUND },
  { "altSwitchSoundPlayer", (int)offsetof(WeaponDef, altSwitchSoundPlayer), CSPFT_SOUND },
  { "putawaySound", (int)offsetof(WeaponDef, putawaySound), CSPFT_SOUND },
  { "putawaySoundPlayer", (int)offsetof(WeaponDef, putawaySoundPlayer), CSPFT_SOUND },
  { "bounceSound", (int)offsetof(WeaponDef, bounceSound), WFT_BOUNCE_SOUND },
  { "viewShellEjectEffect", (int)offsetof(WeaponDef, viewShellEjectEffect), CSPFT_FX },
  { "worldShellEjectEffect", (int)offsetof(WeaponDef, worldShellEjectEffect), CSPFT_FX },
  { "viewLastShotEjectEffect", (int)offsetof(WeaponDef, viewLastShotEjectEffect), CSPFT_FX },
  { "worldLastShotEjectEffect", (int)offsetof(WeaponDef, worldLastShotEjectEffect), CSPFT_FX },
  { "reticleCenter", (int)offsetof(WeaponDef, reticleCenter), CSPFT_MATERIAL },
  { "reticleSide", (int)offsetof(WeaponDef, reticleSide), CSPFT_MATERIAL },
  { "reticleCenterSize", (int)offsetof(WeaponDef, iReticleCenterSize), CSPFT_INT },
  { "reticleSideSize", (int)offsetof(WeaponDef, iReticleSideSize), CSPFT_INT },
  { "reticleMinOfs", (int)offsetof(WeaponDef, iReticleMinOfs), CSPFT_INT },
  { "activeReticleType", (int)offsetof(WeaponDef, activeReticleType), WFT_ACTIVE_RETICLE_TYPE },
  { "standMoveF", (int)offsetof(WeaponDef, vStandMove[0]), CSPFT_FLOAT },
  { "standMoveR", (int)offsetof(WeaponDef, vStandMove[1]), CSPFT_FLOAT },
  { "standMoveU", (int)offsetof(WeaponDef, vStandMove[2]), CSPFT_FLOAT },
  { "standRotP", (int)offsetof(WeaponDef, vStandRot[0]), CSPFT_FLOAT },
  { "standRotY", (int)offsetof(WeaponDef, vStandRot[1]), CSPFT_FLOAT },
  { "standRotR", (int)offsetof(WeaponDef, vStandRot[2]), CSPFT_FLOAT },
  { "duckedOfsF", (int)offsetof(WeaponDef, vDuckedOfs[0]), CSPFT_FLOAT },
  { "duckedOfsR", (int)offsetof(WeaponDef, vDuckedOfs[1]), CSPFT_FLOAT },
  { "duckedOfsU", (int)offsetof(WeaponDef, vDuckedOfs[2]), CSPFT_FLOAT },
  { "duckedMoveF", (int)offsetof(WeaponDef, vDuckedMove[0]), CSPFT_FLOAT },
  { "duckedMoveR", (int)offsetof(WeaponDef, vDuckedMove[1]), CSPFT_FLOAT },
  { "duckedMoveU", (int)offsetof(WeaponDef, vDuckedMove[2]), CSPFT_FLOAT },
  { "duckedRotP", (int)offsetof(WeaponDef, vDuckedRot[0]), CSPFT_FLOAT },
  { "duckedRotY", (int)offsetof(WeaponDef, vDuckedRot[1]), CSPFT_FLOAT },
  { "duckedRotR", (int)offsetof(WeaponDef, vDuckedRot[2]), CSPFT_FLOAT },
  { "proneOfsF", (int)offsetof(WeaponDef, vProneOfs[0]), CSPFT_FLOAT },
  { "proneOfsR", (int)offsetof(WeaponDef, vProneOfs[1]), CSPFT_FLOAT },
  { "proneOfsU", (int)offsetof(WeaponDef, vProneOfs[2]), CSPFT_FLOAT },
  { "proneMoveF", (int)offsetof(WeaponDef, vProneMove[0]), CSPFT_FLOAT },
  { "proneMoveR", (int)offsetof(WeaponDef, vProneMove[1]), CSPFT_FLOAT },
  { "proneMoveU", (int)offsetof(WeaponDef, vProneMove[2]), CSPFT_FLOAT },
  { "proneRotP", (int)offsetof(WeaponDef, vProneRot[0]), CSPFT_FLOAT },
  { "proneRotY", (int)offsetof(WeaponDef, vProneRot[1]), CSPFT_FLOAT },
  { "proneRotR", (int)offsetof(WeaponDef, vProneRot[2]), CSPFT_FLOAT },
  { "posMoveRate", (int)offsetof(WeaponDef, fPosMoveRate), CSPFT_FLOAT },
  { "posProneMoveRate", (int)offsetof(WeaponDef, fPosProneMoveRate), CSPFT_FLOAT },
  { "standMoveMinSpeed", (int)offsetof(WeaponDef, fStandMoveMinSpeed), CSPFT_FLOAT },
  { "duckedMoveMinSpeed", (int)offsetof(WeaponDef, fDuckedMoveMinSpeed), CSPFT_FLOAT },
  { "proneMoveMinSpeed", (int)offsetof(WeaponDef, fProneMoveMinSpeed), CSPFT_FLOAT },
  { "posRotRate", (int)offsetof(WeaponDef, fPosRotRate), CSPFT_FLOAT },
  { "posProneRotRate", (int)offsetof(WeaponDef, fPosProneRotRate), CSPFT_FLOAT },
  { "standRotMinSpeed", (int)offsetof(WeaponDef, fStandRotMinSpeed), CSPFT_FLOAT },
  { "duckedRotMinSpeed", (int)offsetof(WeaponDef, fDuckedRotMinSpeed), CSPFT_FLOAT },
  { "proneRotMinSpeed", (int)offsetof(WeaponDef, fProneRotMinSpeed), CSPFT_FLOAT },
  { "worldModel", (int)offsetof(WeaponDef, worldModel[0]), CSPFT_XMODEL },
  { "worldModel2", (int)offsetof(WeaponDef, worldModel[1]), CSPFT_XMODEL },
  { "worldModel3", (int)offsetof(WeaponDef, worldModel[2]), CSPFT_XMODEL },
  { "worldModel4", (int)offsetof(WeaponDef, worldModel[3]), CSPFT_XMODEL },
  { "worldModel5", (int)offsetof(WeaponDef, worldModel[4]), CSPFT_XMODEL },
  { "worldModel6", (int)offsetof(WeaponDef, worldModel[5]), CSPFT_XMODEL },
  { "worldModel7", (int)offsetof(WeaponDef, worldModel[6]), CSPFT_XMODEL },
  { "worldModel8", (int)offsetof(WeaponDef, worldModel[7]), CSPFT_XMODEL },
  { "worldModel9", (int)offsetof(WeaponDef, worldModel[8]), CSPFT_XMODEL },
  { "worldModel10", (int)offsetof(WeaponDef, worldModel[9]), CSPFT_XMODEL },
  { "worldModel11", (int)offsetof(WeaponDef, worldModel[10]), CSPFT_XMODEL },
  { "worldModel12", (int)offsetof(WeaponDef, worldModel[11]), CSPFT_XMODEL },
  { "worldModel13", (int)offsetof(WeaponDef, worldModel[12]), CSPFT_XMODEL },
  { "worldModel14", (int)offsetof(WeaponDef, worldModel[13]), CSPFT_XMODEL },
  { "worldModel15", (int)offsetof(WeaponDef, worldModel[14]), CSPFT_XMODEL },
  { "worldModel16", (int)offsetof(WeaponDef, worldModel[15]), CSPFT_XMODEL },
  { "worldClipModel", (int)offsetof(WeaponDef, worldClipModel), CSPFT_XMODEL },
  { "rocketModel", (int)offsetof(WeaponDef, rocketModel), CSPFT_XMODEL },
  { "knifeModel", (int)offsetof(WeaponDef, knifeModel), CSPFT_XMODEL },
  { "worldKnifeModel", (int)offsetof(WeaponDef, worldKnifeModel), CSPFT_XMODEL },
  { "hudIcon", (int)offsetof(WeaponDef, hudIcon), CSPFT_MATERIAL },
  { "hudIconRatio", (int)offsetof(WeaponDef, hudIconRatio), WFT_ICONRATIO_HUD },
  { "ammoCounterIcon", (int)offsetof(WeaponDef, ammoCounterIcon), CSPFT_MATERIAL },
  { "ammoCounterIconRatio", (int)offsetof(WeaponDef, ammoCounterIconRatio), WFT_ICONRATIO_AMMOCOUNTER },
  { "ammoCounterClip", (int)offsetof(WeaponDef, ammoCounterClip), WFT_AMMOCOUNTER_CLIPTYPE },
  { "startAmmo", (int)offsetof(WeaponDef, iStartAmmo), CSPFT_INT },
  { "ammoName", (int)offsetof(WeaponDef, szAmmoName), CSPFT_STRING },
  { "clipName", (int)offsetof(WeaponDef, szClipName), CSPFT_STRING },
  { "maxAmmo", (int)offsetof(WeaponDef, iMaxAmmo), CSPFT_INT },
  { "clipSize", (int)offsetof(WeaponDef, iClipSize), CSPFT_INT },
  { "shotCount", (int)offsetof(WeaponDef, shotCount), CSPFT_INT },
  { "sharedAmmoCapName", (int)offsetof(WeaponDef, szSharedAmmoCapName), CSPFT_STRING },
  { "sharedAmmoCap", (int)offsetof(WeaponDef, iSharedAmmoCap), CSPFT_INT },
  { "damage", (int)offsetof(WeaponDef, damage), CSPFT_INT },
  { "playerDamage", (int)offsetof(WeaponDef, playerDamage), CSPFT_INT },
  { "meleeDamage", (int)offsetof(WeaponDef, iMeleeDamage), CSPFT_INT },
  { "minDamage", (int)offsetof(WeaponDef, minDamage), CSPFT_INT },
  { "minPlayerDamage", (int)offsetof(WeaponDef, minPlayerDamage), CSPFT_INT },
  { "maxDamageRange", (int)offsetof(WeaponDef, fMaxDamageRange), CSPFT_FLOAT },
  { "minDamageRange", (int)offsetof(WeaponDef, fMinDamageRange), CSPFT_FLOAT },
  { "destabilizationRateTime", (int)offsetof(WeaponDef, destabilizationRateTime), CSPFT_FLOAT },
  { "destabilizationCurvatureMax", (int)offsetof(WeaponDef, destabilizationCurvatureMax), CSPFT_FLOAT },
  { "destabilizeDistance", (int)offsetof(WeaponDef, destabilizeDistance), CSPFT_INT },
  { "fireDelay", (int)offsetof(WeaponDef, iFireDelay), CSPFT_MILLISECONDS },
  { "meleeDelay", (int)offsetof(WeaponDef, iMeleeDelay), CSPFT_MILLISECONDS },
  { "meleeChargeDelay", (int)offsetof(WeaponDef, meleeChargeDelay), CSPFT_MILLISECONDS },
  { "fireTime", (int)offsetof(WeaponDef, iFireTime), CSPFT_MILLISECONDS },
  { "rechamberTime", (int)offsetof(WeaponDef, iRechamberTime), CSPFT_MILLISECONDS },
  { "rechamberBoltTime", (int)offsetof(WeaponDef, iRechamberBoltTime), CSPFT_MILLISECONDS },
  { "holdFireTime", (int)offsetof(WeaponDef, iHoldFireTime), CSPFT_MILLISECONDS },
  { "detonateTime", (int)offsetof(WeaponDef, iDetonateTime), CSPFT_MILLISECONDS },
  { "detonateDelay", (int)offsetof(WeaponDef, iDetonateDelay), CSPFT_MILLISECONDS },
  { "meleeTime", (int)offsetof(WeaponDef, iMeleeTime), CSPFT_MILLISECONDS },
  { "meleeChargeTime", (int)offsetof(WeaponDef, meleeChargeTime), CSPFT_MILLISECONDS },
  { "reloadTime", (int)offsetof(WeaponDef, iReloadTime), CSPFT_MILLISECONDS },
  { "reloadShowRocketTime", (int)offsetof(WeaponDef, reloadShowRocketTime), CSPFT_MILLISECONDS },
  { "reloadEmptyTime", (int)offsetof(WeaponDef, iReloadEmptyTime), CSPFT_MILLISECONDS },
  { "reloadAddTime", (int)offsetof(WeaponDef, iReloadAddTime), CSPFT_MILLISECONDS },
  { "reloadStartTime", (int)offsetof(WeaponDef, iReloadStartTime), CSPFT_MILLISECONDS },
  { "reloadStartAddTime", (int)offsetof(WeaponDef, iReloadStartAddTime), CSPFT_MILLISECONDS },
  { "reloadEndTime", (int)offsetof(WeaponDef, iReloadEndTime), CSPFT_MILLISECONDS },
  { "dropTime", (int)offsetof(WeaponDef, iDropTime), CSPFT_MILLISECONDS },
  { "raiseTime", (int)offsetof(WeaponDef, iRaiseTime), CSPFT_MILLISECONDS },
  { "altDropTime", (int)offsetof(WeaponDef, iAltDropTime), CSPFT_MILLISECONDS },
  { "altRaiseTime", (int)offsetof(WeaponDef, iAltRaiseTime), CSPFT_MILLISECONDS },
  { "quickDropTime", (int)offsetof(WeaponDef, quickDropTime), CSPFT_MILLISECONDS },
  { "quickRaiseTime", (int)offsetof(WeaponDef, quickRaiseTime), CSPFT_MILLISECONDS },
  { "firstRaiseTime", (int)offsetof(WeaponDef, iFirstRaiseTime), CSPFT_MILLISECONDS },
  { "emptyRaiseTime", (int)offsetof(WeaponDef, iEmptyRaiseTime), CSPFT_MILLISECONDS },
  { "emptyDropTime", (int)offsetof(WeaponDef, iEmptyDropTime), CSPFT_MILLISECONDS },
  { "sprintInTime", (int)offsetof(WeaponDef, sprintInTime), CSPFT_MILLISECONDS },
  { "sprintLoopTime", (int)offsetof(WeaponDef, sprintLoopTime), CSPFT_MILLISECONDS },
  { "sprintOutTime", (int)offsetof(WeaponDef, sprintOutTime), CSPFT_MILLISECONDS },
  { "nightVisionWearTime", (int)offsetof(WeaponDef, nightVisionWearTime), CSPFT_MILLISECONDS },
  { "nightVisionWearTimeFadeOutEnd", (int)offsetof(WeaponDef, nightVisionWearTimeFadeOutEnd), CSPFT_MILLISECONDS },
  { "nightVisionWearTimePowerUp", (int)offsetof(WeaponDef, nightVisionWearTimePowerUp), CSPFT_MILLISECONDS },
  { "nightVisionRemoveTime", (int)offsetof(WeaponDef, nightVisionRemoveTime), CSPFT_MILLISECONDS },
  { "nightVisionRemoveTimePowerDown", (int)offsetof(WeaponDef, nightVisionRemoveTimePowerDown), CSPFT_MILLISECONDS },
  { "nightVisionRemoveTimeFadeInStart", (int)offsetof(WeaponDef, nightVisionRemoveTimeFadeInStart), CSPFT_MILLISECONDS },
  { "fuseTime", (int)offsetof(WeaponDef, fuseTime), CSPFT_MILLISECONDS },
  { "aifuseTime", (int)offsetof(WeaponDef, aiFuseTime), CSPFT_MILLISECONDS },
  { "requireLockonToFire", (int)offsetof(WeaponDef, requireLockonToFire), CSPFT_QBOOLEAN },
  { "noAdsWhenMagEmpty", (int)offsetof(WeaponDef, noAdsWhenMagEmpty), CSPFT_QBOOLEAN },
  { "avoidDropCleanup", (int)offsetof(WeaponDef, avoidDropCleanup), CSPFT_QBOOLEAN },
  { "autoAimRange", (int)offsetof(WeaponDef, autoAimRange), CSPFT_FLOAT },
  { "aimAssistRange", (int)offsetof(WeaponDef, aimAssistRange), CSPFT_FLOAT },
  { "aimAssistRangeAds", (int)offsetof(WeaponDef, aimAssistRangeAds), CSPFT_FLOAT },
  { "aimPadding", (int)offsetof(WeaponDef, aimPadding), CSPFT_FLOAT },
  { "enemyCrosshairRange", (int)offsetof(WeaponDef, enemyCrosshairRange), CSPFT_FLOAT },
  { "crosshairColorChange", (int)offsetof(WeaponDef, crosshairColorChange), CSPFT_QBOOLEAN },
  { "moveSpeedScale", (int)offsetof(WeaponDef, moveSpeedScale), CSPFT_FLOAT },
  { "adsMoveSpeedScale", (int)offsetof(WeaponDef, adsMoveSpeedScale), CSPFT_FLOAT },
  { "sprintDurationScale", (int)offsetof(WeaponDef, sprintDurationScale), CSPFT_FLOAT },
  { "idleCrouchFactor", (int)offsetof(WeaponDef, fIdleCrouchFactor), CSPFT_FLOAT },
  { "idleProneFactor", (int)offsetof(WeaponDef, fIdleProneFactor), CSPFT_FLOAT },
  { "gunMaxPitch", (int)offsetof(WeaponDef, fGunMaxPitch), CSPFT_FLOAT },
  { "gunMaxYaw", (int)offsetof(WeaponDef, fGunMaxYaw), CSPFT_FLOAT },
  { "swayMaxAngle", (int)offsetof(WeaponDef, swayMaxAngle), CSPFT_FLOAT },
  { "swayLerpSpeed", (int)offsetof(WeaponDef, swayLerpSpeed), CSPFT_FLOAT },
  { "swayPitchScale", (int)offsetof(WeaponDef, swayPitchScale), CSPFT_FLOAT },
  { "swayYawScale", (int)offsetof(WeaponDef, swayYawScale), CSPFT_FLOAT },
  { "swayHorizScale", (int)offsetof(WeaponDef, swayHorizScale), CSPFT_FLOAT },
  { "swayVertScale", (int)offsetof(WeaponDef, swayVertScale), CSPFT_FLOAT },
  { "swayShellShockScale", (int)offsetof(WeaponDef, swayShellShockScale), CSPFT_FLOAT },
  { "adsSwayMaxAngle", (int)offsetof(WeaponDef, adsSwayMaxAngle), CSPFT_FLOAT },
  { "adsSwayLerpSpeed", (int)offsetof(WeaponDef, adsSwayLerpSpeed), CSPFT_FLOAT },
  { "adsSwayPitchScale", (int)offsetof(WeaponDef, adsSwayPitchScale), CSPFT_FLOAT },
  { "adsSwayYawScale", (int)offsetof(WeaponDef, adsSwayYawScale), CSPFT_FLOAT },
  { "adsSwayHorizScale", (int)offsetof(WeaponDef, adsSwayHorizScale), CSPFT_FLOAT },
  { "adsSwayVertScale", (int)offsetof(WeaponDef, adsSwayVertScale), CSPFT_FLOAT },
  { "rifleBullet", (int)offsetof(WeaponDef, bRifleBullet), CSPFT_QBOOLEAN },
  { "armorPiercing", (int)offsetof(WeaponDef, armorPiercing), CSPFT_QBOOLEAN },
  { "boltAction", (int)offsetof(WeaponDef, bBoltAction), CSPFT_QBOOLEAN },
  { "aimDownSight", (int)offsetof(WeaponDef, aimDownSight), CSPFT_QBOOLEAN },
  { "rechamberWhileAds", (int)offsetof(WeaponDef, bRechamberWhileAds), CSPFT_QBOOLEAN },
  { "adsViewErrorMin", (int)offsetof(WeaponDef, adsViewErrorMin), CSPFT_FLOAT },
  { "adsViewErrorMax", (int)offsetof(WeaponDef, adsViewErrorMax), CSPFT_FLOAT },
  { "clipOnly", (int)offsetof(WeaponDef, bClipOnly), CSPFT_QBOOLEAN },
  { "cookOffHold", (int)offsetof(WeaponDef, bCookOffHold), CSPFT_QBOOLEAN },
  { "adsFire", (int)offsetof(WeaponDef, adsFireOnly), CSPFT_QBOOLEAN },
  { "cancelAutoHolsterWhenEmpty", (int)offsetof(WeaponDef, cancelAutoHolsterWhenEmpty), CSPFT_QBOOLEAN },
  { "suppressAmmoReserveDisplay", (int)offsetof(WeaponDef, suppressAmmoReserveDisplay), CSPFT_QBOOLEAN },
  { "enhanced", (int)offsetof(WeaponDef, enhanced), CSPFT_QBOOLEAN },
  { "laserSightDuringNightvision", (int)offsetof(WeaponDef, laserSightDuringNightvision), CSPFT_QBOOLEAN },
  { "killIcon", (int)offsetof(WeaponDef, killIcon), CSPFT_MATERIAL },
  { "killIconRatio", (int)offsetof(WeaponDef, killIconRatio), WFT_ICONRATIO_KILL },
  { "flipKillIcon", (int)offsetof(WeaponDef, flipKillIcon), CSPFT_QBOOLEAN },
  { "dpadIcon", (int)offsetof(WeaponDef, dpadIcon), CSPFT_MATERIAL },
  { "dpadIconRatio", (int)offsetof(WeaponDef, dpadIconRatio), WFT_ICONRATIO_DPAD },
  { "noPartialReload", (int)offsetof(WeaponDef, bNoPartialReload), CSPFT_QBOOLEAN },
  { "segmentedReload", (int)offsetof(WeaponDef, bSegmentedReload), CSPFT_QBOOLEAN },
  { "reloadAmmoAdd", (int)offsetof(WeaponDef, iReloadAmmoAdd), CSPFT_INT },
  { "reloadStartAdd", (int)offsetof(WeaponDef, iReloadStartAdd), CSPFT_INT },
  { "altWeapon", (int)offsetof(WeaponDef, szAltWeaponName), CSPFT_STRING },
  { "dropAmmoMin", (int)offsetof(WeaponDef, iDropAmmoMin), CSPFT_INT },
  { "dropAmmoMax", (int)offsetof(WeaponDef, iDropAmmoMax), CSPFT_INT },
  { "blocksProne", (int)offsetof(WeaponDef, blocksProne), CSPFT_QBOOLEAN },
  { "silenced", (int)offsetof(WeaponDef, silenced), CSPFT_QBOOLEAN },
  { "explosionRadius", (int)offsetof(WeaponDef, iExplosionRadius), CSPFT_INT },
  { "explosionRadiusMin", (int)offsetof(WeaponDef, iExplosionRadiusMin), CSPFT_INT },
  { "explosionInnerDamage", (int)offsetof(WeaponDef, iExplosionInnerDamage), CSPFT_INT },
  { "explosionOuterDamage", (int)offsetof(WeaponDef, iExplosionOuterDamage), CSPFT_INT },
  { "damageConeAngle", (int)offsetof(WeaponDef, damageConeAngle), CSPFT_FLOAT },
  { "projectileSpeed", (int)offsetof(WeaponDef, iProjectileSpeed), CSPFT_INT },
  { "projectileSpeedUp", (int)offsetof(WeaponDef, iProjectileSpeedUp), CSPFT_INT },
  { "projectileSpeedForward", (int)offsetof(WeaponDef, iProjectileSpeedForward), CSPFT_INT },
  { "projectileActivateDist", (int)offsetof(WeaponDef, iProjectileActivateDist), CSPFT_INT },
  { "projectileLifetime", (int)offsetof(WeaponDef, projLifetime), CSPFT_FLOAT },
  { "timeToAccelerate", (int)offsetof(WeaponDef, timeToAccelerate), CSPFT_FLOAT },
  { "projectileCurvature", (int)offsetof(WeaponDef, projectileCurvature), CSPFT_FLOAT },
  { "projectileModel", (int)offsetof(WeaponDef, projectileModel), CSPFT_XMODEL },
  { "projExplosionType", (int)offsetof(WeaponDef, projExplosion), WFT_PROJ_EXPLOSION },
  { "projExplosionEffect", (int)offsetof(WeaponDef, projExplosionEffect), CSPFT_FX },
  { "projExplosionEffectForceNormalUp", (int)offsetof(WeaponDef, projExplosionEffectForceNormalUp), CSPFT_QBOOLEAN },
  { "projExplosionSound", (int)offsetof(WeaponDef, projExplosionSound), CSPFT_SOUND },
  { "projDudEffect", (int)offsetof(WeaponDef, projDudEffect), CSPFT_FX },
  { "projDudSound", (int)offsetof(WeaponDef, projDudSound), CSPFT_SOUND },
  { "projImpactExplode", (int)offsetof(WeaponDef, bProjImpactExplode), CSPFT_QBOOLEAN },
  { "stickiness", (int)offsetof(WeaponDef, stickiness), WFT_STICKINESS },
  { "hasDetonator", (int)offsetof(WeaponDef, hasDetonator), CSPFT_QBOOLEAN },
  { "timedDetonation", (int)offsetof(WeaponDef, timedDetonation), CSPFT_QBOOLEAN },
  { "rotate", (int)offsetof(WeaponDef, rotate), CSPFT_QBOOLEAN },
  { "holdButtonToThrow", (int)offsetof(WeaponDef, holdButtonToThrow), CSPFT_QBOOLEAN },
  { "freezeMovementWhenFiring", (int)offsetof(WeaponDef, freezeMovementWhenFiring), CSPFT_QBOOLEAN },
  { "lowAmmoWarningThreshold", (int)offsetof(WeaponDef, lowAmmoWarningThreshold), CSPFT_FLOAT },
  { "parallelDefaultBounce", (int)offsetof(WeaponDef, parallelBounce[0]), CSPFT_FLOAT },
  { "parallelBarkBounce", (int)offsetof(WeaponDef, parallelBounce[1]), CSPFT_FLOAT },
  { "parallelBrickBounce", (int)offsetof(WeaponDef, parallelBounce[2]), CSPFT_FLOAT },
  { "parallelCarpetBounce", (int)offsetof(WeaponDef, parallelBounce[3]), CSPFT_FLOAT },
  { "parallelClothBounce", (int)offsetof(WeaponDef, parallelBounce[4]), CSPFT_FLOAT },
  { "parallelConcreteBounce", (int)offsetof(WeaponDef, parallelBounce[5]), CSPFT_FLOAT },
  { "parallelDirtBounce", (int)offsetof(WeaponDef, parallelBounce[6]), CSPFT_FLOAT },
  { "parallelFleshBounce", (int)offsetof(WeaponDef, parallelBounce[7]), CSPFT_FLOAT },
  { "parallelFoliageBounce", (int)offsetof(WeaponDef, parallelBounce[8]), CSPFT_FLOAT },
  { "parallelGlassBounce", (int)offsetof(WeaponDef, parallelBounce[9]), CSPFT_FLOAT },
  { "parallelGrassBounce", (int)offsetof(WeaponDef, parallelBounce[10]), CSPFT_FLOAT },
  { "parallelGravelBounce", (int)offsetof(WeaponDef, parallelBounce[11]), CSPFT_FLOAT },
  { "parallelIceBounce", (int)offsetof(WeaponDef, parallelBounce[12]), CSPFT_FLOAT },
  { "parallelMetalBounce", (int)offsetof(WeaponDef, parallelBounce[13]), CSPFT_FLOAT },
  { "parallelMudBounce", (int)offsetof(WeaponDef, parallelBounce[14]), CSPFT_FLOAT },
  { "parallelPaperBounce", (int)offsetof(WeaponDef, parallelBounce[15]), CSPFT_FLOAT },
  { "parallelPlasterBounce", (int)offsetof(WeaponDef, parallelBounce[16]), CSPFT_FLOAT },
  { "parallelRockBounce", (int)offsetof(WeaponDef, parallelBounce[17]), CSPFT_FLOAT },
  { "parallelSandBounce", (int)offsetof(WeaponDef, parallelBounce[18]), CSPFT_FLOAT },
  { "parallelSnowBounce", (int)offsetof(WeaponDef, parallelBounce[19]), CSPFT_FLOAT },
  { "parallelWaterBounce", (int)offsetof(WeaponDef, parallelBounce[20]), CSPFT_FLOAT },
  { "parallelWoodBounce", (int)offsetof(WeaponDef, parallelBounce[21]), CSPFT_FLOAT },
  { "parallelAsphaltBounce", (int)offsetof(WeaponDef, parallelBounce[22]), CSPFT_FLOAT },
  { "parallelCeramicBounce", (int)offsetof(WeaponDef, parallelBounce[23]), CSPFT_FLOAT },
  { "parallelPlasticBounce", (int)offsetof(WeaponDef, parallelBounce[24]), CSPFT_FLOAT },
  { "parallelRubberBounce", (int)offsetof(WeaponDef, parallelBounce[25]), CSPFT_FLOAT },
  { "parallelCushionBounce", (int)offsetof(WeaponDef, parallelBounce[26]), CSPFT_FLOAT },
  { "parallelFruitBounce", (int)offsetof(WeaponDef, parallelBounce[27]), CSPFT_FLOAT },
  { "parallelPaintedMetalBounce", (int)offsetof(WeaponDef, parallelBounce[28]), CSPFT_FLOAT },
  { "perpendicularDefaultBounce", (int)offsetof(WeaponDef, perpendicularBounce[0]), CSPFT_FLOAT },
  { "perpendicularBarkBounce", (int)offsetof(WeaponDef, perpendicularBounce[1]), CSPFT_FLOAT },
  { "perpendicularBrickBounce", (int)offsetof(WeaponDef, perpendicularBounce[2]), CSPFT_FLOAT },
  { "perpendicularCarpetBounce", (int)offsetof(WeaponDef, perpendicularBounce[3]), CSPFT_FLOAT },
  { "perpendicularClothBounce", (int)offsetof(WeaponDef, perpendicularBounce[4]), CSPFT_FLOAT },
  { "perpendicularConcreteBounce", (int)offsetof(WeaponDef, perpendicularBounce[5]), CSPFT_FLOAT },
  { "perpendicularDirtBounce", (int)offsetof(WeaponDef, perpendicularBounce[6]), CSPFT_FLOAT },
  { "perpendicularFleshBounce", (int)offsetof(WeaponDef, perpendicularBounce[7]), CSPFT_FLOAT },
  { "perpendicularFoliageBounce", (int)offsetof(WeaponDef, perpendicularBounce[8]), CSPFT_FLOAT },
  { "perpendicularGlassBounce", (int)offsetof(WeaponDef, perpendicularBounce[9]), CSPFT_FLOAT },
  { "perpendicularGrassBounce", (int)offsetof(WeaponDef, perpendicularBounce[10]), CSPFT_FLOAT },
  { "perpendicularGravelBounce", (int)offsetof(WeaponDef, perpendicularBounce[11]), CSPFT_FLOAT },
  { "perpendicularIceBounce", (int)offsetof(WeaponDef, perpendicularBounce[12]), CSPFT_FLOAT },
  { "perpendicularMetalBounce", (int)offsetof(WeaponDef, perpendicularBounce[13]), CSPFT_FLOAT },
  { "perpendicularMudBounce", (int)offsetof(WeaponDef, perpendicularBounce[14]), CSPFT_FLOAT },
  { "perpendicularPaperBounce", (int)offsetof(WeaponDef, perpendicularBounce[15]), CSPFT_FLOAT },
  { "perpendicularPlasterBounce", (int)offsetof(WeaponDef, perpendicularBounce[16]), CSPFT_FLOAT },
  { "perpendicularRockBounce", (int)offsetof(WeaponDef, perpendicularBounce[17]), CSPFT_FLOAT },
  { "perpendicularSandBounce", (int)offsetof(WeaponDef, perpendicularBounce[18]), CSPFT_FLOAT },
  { "perpendicularSnowBounce", (int)offsetof(WeaponDef, perpendicularBounce[19]), CSPFT_FLOAT },
  { "perpendicularWaterBounce", (int)offsetof(WeaponDef, perpendicularBounce[20]), CSPFT_FLOAT },
  { "perpendicularWoodBounce", (int)offsetof(WeaponDef, perpendicularBounce[21]), CSPFT_FLOAT },
  { "perpendicularAsphaltBounce", (int)offsetof(WeaponDef, perpendicularBounce[22]), CSPFT_FLOAT },
  { "perpendicularCeramicBounce", (int)offsetof(WeaponDef, parallelBounce[23]), CSPFT_FLOAT },
  { "perpendicularPlasticBounce", (int)offsetof(WeaponDef, parallelBounce[24]), CSPFT_FLOAT },
  { "perpendicularRubberBounce", (int)offsetof(WeaponDef, parallelBounce[25]), CSPFT_FLOAT },
  { "perpendicularCushionBounce", (int)offsetof(WeaponDef, perpendicularBounce[26]), CSPFT_FLOAT },
  { "perpendicularFruitBounce", (int)offsetof(WeaponDef, perpendicularBounce[27]), CSPFT_FLOAT },
  { "perpendicularPaintedMetalBounce", (int)offsetof(WeaponDef, perpendicularBounce[28]), CSPFT_FLOAT },
  { "projTrailEffect", (int)offsetof(WeaponDef, projTrailEffect), CSPFT_FX },
  { "projectileRed", (int)offsetof(WeaponDef, vProjectileColor[0]), CSPFT_FLOAT },
  { "projectileGreen", (int)offsetof(WeaponDef, vProjectileColor[1]), CSPFT_FLOAT },
  { "projectileBlue", (int)offsetof(WeaponDef, vProjectileColor[2]), CSPFT_FLOAT },
  { "guidedMissileType", (int)offsetof(WeaponDef, guidedMissileType), WFT_GUIDED_MISSILE_TYPE },
  { "maxSteeringAccel", (int)offsetof(WeaponDef, maxSteeringAccel), CSPFT_FLOAT },
  { "projIgnitionDelay", (int)offsetof(WeaponDef, projIgnitionDelay), CSPFT_INT },
  { "projIgnitionEffect", (int)offsetof(WeaponDef, projIgnitionEffect), CSPFT_FX },
  { "projIgnitionSound", (int)offsetof(WeaponDef, projIgnitionSound), CSPFT_SOUND },
  { "adsTransInTime", (int)offsetof(WeaponDef, iAdsTransInTime), CSPFT_MILLISECONDS },
  { "adsTransOutTime", (int)offsetof(WeaponDef, iAdsTransOutTime), CSPFT_MILLISECONDS },
  { "adsIdleAmount", (int)offsetof(WeaponDef, fAdsIdleAmount), CSPFT_FLOAT },
  { "adsIdleSpeed", (int)offsetof(WeaponDef, adsIdleSpeed), CSPFT_FLOAT },
  { "adsZoomFov", (int)offsetof(WeaponDef, fAdsZoomFov), CSPFT_FLOAT },
  { "adsZoomInFrac", (int)offsetof(WeaponDef, fAdsZoomInFrac), CSPFT_FLOAT },
  { "adsZoomOutFrac", (int)offsetof(WeaponDef, fAdsZoomOutFrac), CSPFT_FLOAT },
  { "adsOverlayShader", (int)offsetof(WeaponDef, overlayMaterial), CSPFT_MATERIAL },
  { "adsOverlayShaderLowRes", (int)offsetof(WeaponDef, overlayMaterialLowRes), CSPFT_MATERIAL },
  { "adsOverlayReticle", (int)offsetof(WeaponDef, overlayReticle), WFT_OVERLAYRETICLE },
  { "adsOverlayInterface", (int)offsetof(WeaponDef, overlayInterface), WFT_OVERLAYINTERFACE },
  { "adsOverlayWidth", (int)offsetof(WeaponDef, overlayWidth), CSPFT_FLOAT },
  { "adsOverlayHeight", (int)offsetof(WeaponDef, overlayHeight), CSPFT_FLOAT },
  { "adsBobFactor", (int)offsetof(WeaponDef, fAdsBobFactor), CSPFT_FLOAT },
  { "adsViewBobMult", (int)offsetof(WeaponDef, fAdsViewBobMult), CSPFT_FLOAT },
  { "adsAimPitch", (int)offsetof(WeaponDef, fAdsAimPitch), CSPFT_FLOAT },
  { "adsCrosshairInFrac", (int)offsetof(WeaponDef, fAdsCrosshairInFrac), CSPFT_FLOAT },
  { "adsCrosshairOutFrac", (int)offsetof(WeaponDef, fAdsCrosshairOutFrac), CSPFT_FLOAT },
  { "adsReloadTransTime", (int)offsetof(WeaponDef, iPositionReloadTransTime), CSPFT_MILLISECONDS },
  { "adsGunKickReducedKickBullets", (int)offsetof(WeaponDef, adsGunKickReducedKickBullets), CSPFT_INT },
  { "adsGunKickReducedKickPercent", (int)offsetof(WeaponDef, adsGunKickReducedKickPercent), CSPFT_FLOAT },
  { "adsGunKickPitchMin", (int)offsetof(WeaponDef, fAdsGunKickPitchMin), CSPFT_FLOAT },
  { "adsGunKickPitchMax", (int)offsetof(WeaponDef, fAdsGunKickPitchMax), CSPFT_FLOAT },
  { "adsGunKickYawMin", (int)offsetof(WeaponDef, fAdsGunKickYawMin), CSPFT_FLOAT },
  { "adsGunKickYawMax", (int)offsetof(WeaponDef, fAdsGunKickYawMax), CSPFT_FLOAT },
  { "adsGunKickAccel", (int)offsetof(WeaponDef, fAdsGunKickAccel), CSPFT_FLOAT },
  { "adsGunKickSpeedMax", (int)offsetof(WeaponDef, fAdsGunKickSpeedMax), CSPFT_FLOAT },
  { "adsGunKickSpeedDecay", (int)offsetof(WeaponDef, fAdsGunKickSpeedDecay), CSPFT_FLOAT },
  { "adsGunKickStaticDecay", (int)offsetof(WeaponDef, fAdsGunKickStaticDecay), CSPFT_FLOAT },
  { "adsViewKickPitchMin", (int)offsetof(WeaponDef, fAdsViewKickPitchMin), CSPFT_FLOAT },
  { "adsViewKickPitchMax", (int)offsetof(WeaponDef, fAdsViewKickPitchMax), CSPFT_FLOAT },
  { "adsViewKickYawMin", (int)offsetof(WeaponDef, fAdsViewKickYawMin), CSPFT_FLOAT },
  { "adsViewKickYawMax", (int)offsetof(WeaponDef, fAdsViewKickYawMax), CSPFT_FLOAT },
  { "adsViewKickCenterSpeed", (int)offsetof(WeaponDef, fAdsViewKickCenterSpeed), CSPFT_FLOAT },
  { "adsSpread", (int)offsetof(WeaponDef, fAdsSpread), CSPFT_FLOAT },
  { "guidedMissileType", (int)offsetof(WeaponDef, guidedMissileType), WFT_GUIDED_MISSILE_TYPE },
  { "hipSpreadStandMin", (int)offsetof(WeaponDef, fHipSpreadStandMin), CSPFT_FLOAT },
  { "hipSpreadDuckedMin", (int)offsetof(WeaponDef, fHipSpreadDuckedMin), CSPFT_FLOAT },
  { "hipSpreadProneMin", (int)offsetof(WeaponDef, fHipSpreadProneMin), CSPFT_FLOAT },
  { "hipSpreadMax", (int)offsetof(WeaponDef, hipSpreadStandMax), CSPFT_FLOAT },
  { "hipSpreadDuckedMax", (int)offsetof(WeaponDef, hipSpreadDuckedMax), CSPFT_FLOAT },
  { "hipSpreadProneMax", (int)offsetof(WeaponDef, hipSpreadProneMax), CSPFT_FLOAT },
  { "hipSpreadDecayRate", (int)offsetof(WeaponDef, fHipSpreadDecayRate), CSPFT_FLOAT },
  { "hipSpreadFireAdd", (int)offsetof(WeaponDef, fHipSpreadFireAdd), CSPFT_FLOAT },
  { "hipSpreadTurnAdd", (int)offsetof(WeaponDef, fHipSpreadTurnAdd), CSPFT_FLOAT },
  { "hipSpreadMoveAdd", (int)offsetof(WeaponDef, fHipSpreadMoveAdd), CSPFT_FLOAT },
  { "hipSpreadDuckedDecay", (int)offsetof(WeaponDef, fHipSpreadDuckedDecay), CSPFT_FLOAT },
  { "hipSpreadProneDecay", (int)offsetof(WeaponDef, fHipSpreadProneDecay), CSPFT_FLOAT },
  { "hipReticleSidePos", (int)offsetof(WeaponDef, fHipReticleSidePos), CSPFT_FLOAT },
  { "hipIdleAmount", (int)offsetof(WeaponDef, fHipIdleAmount), CSPFT_FLOAT },
  { "hipIdleSpeed", (int)offsetof(WeaponDef, hipIdleSpeed), CSPFT_FLOAT },
  { "hipGunKickReducedKickBullets", (int)offsetof(WeaponDef, hipGunKickReducedKickBullets), CSPFT_INT },
  { "hipGunKickReducedKickPercent", (int)offsetof(WeaponDef, hipGunKickReducedKickPercent), CSPFT_FLOAT },
  { "hipGunKickPitchMin", (int)offsetof(WeaponDef, fHipGunKickPitchMin), CSPFT_FLOAT },
  { "hipGunKickPitchMax", (int)offsetof(WeaponDef, fHipGunKickPitchMax), CSPFT_FLOAT },
  { "hipGunKickYawMin", (int)offsetof(WeaponDef, fHipGunKickYawMin), CSPFT_FLOAT },
  { "hipGunKickYawMax", (int)offsetof(WeaponDef, fHipGunKickYawMax), CSPFT_FLOAT },
  { "hipGunKickAccel", (int)offsetof(WeaponDef, fHipGunKickAccel), CSPFT_FLOAT },
  { "hipGunKickSpeedMax", (int)offsetof(WeaponDef, fHipGunKickSpeedMax), CSPFT_FLOAT },
  { "hipGunKickSpeedDecay", (int)offsetof(WeaponDef, fHipGunKickSpeedDecay), CSPFT_FLOAT },
  { "hipGunKickStaticDecay", (int)offsetof(WeaponDef, fHipGunKickStaticDecay), CSPFT_FLOAT },
  { "hipViewKickPitchMin", (int)offsetof(WeaponDef, fHipViewKickPitchMin), CSPFT_FLOAT },
  { "hipViewKickPitchMax", (int)offsetof(WeaponDef, fHipViewKickPitchMax), CSPFT_FLOAT },
  { "hipViewKickYawMin", (int)offsetof(WeaponDef, fHipViewKickYawMin), CSPFT_FLOAT },
  { "hipViewKickYawMax", (int)offsetof(WeaponDef, fHipViewKickYawMax), CSPFT_FLOAT },
  { "hipViewKickCenterSpeed", (int)offsetof(WeaponDef, fHipViewKickCenterSpeed), CSPFT_FLOAT },
  { "leftArc", (int)offsetof(WeaponDef, leftArc), CSPFT_FLOAT },
  { "rightArc", (int)offsetof(WeaponDef, rightArc), CSPFT_FLOAT },
  { "topArc", (int)offsetof(WeaponDef, topArc), CSPFT_FLOAT },
  { "bottomArc", (int)offsetof(WeaponDef, bottomArc), CSPFT_FLOAT },
  { "accuracy", (int)offsetof(WeaponDef, accuracy), CSPFT_FLOAT },
  { "aiSpread", (int)offsetof(WeaponDef, aiSpread), CSPFT_FLOAT },
  { "playerSpread", (int)offsetof(WeaponDef, playerSpread), CSPFT_FLOAT },
  { "maxVertTurnSpeed", (int)offsetof(WeaponDef, maxTurnSpeed[0]), CSPFT_FLOAT },
  { "maxHorTurnSpeed", (int)offsetof(WeaponDef, maxTurnSpeed[1]), CSPFT_FLOAT },
  { "minVertTurnSpeed", (int)offsetof(WeaponDef, minTurnSpeed[0]), CSPFT_FLOAT },
  { "minHorTurnSpeed", (int)offsetof(WeaponDef, minTurnSpeed[1]), CSPFT_FLOAT },
  { "pitchConvergenceTime", (int)offsetof(WeaponDef, pitchConvergenceTime), CSPFT_FLOAT },
  { "yawConvergenceTime", (int)offsetof(WeaponDef, yawConvergenceTime), CSPFT_FLOAT },
  { "suppressionTime", (int)offsetof(WeaponDef, suppressTime), CSPFT_FLOAT },
  { "maxRange", (int)offsetof(WeaponDef, maxRange), CSPFT_FLOAT },
  { "animHorRotateInc", (int)offsetof(WeaponDef, fAnimHorRotateInc), CSPFT_FLOAT },
  { "playerPositionDist", (int)offsetof(WeaponDef, fPlayerPositionDist), CSPFT_FLOAT },
  { "stance", (int)offsetof(WeaponDef, stance), WFT_STANCE },
  { "useHintString", (int)offsetof(WeaponDef, szUseHintString), CSPFT_STRING },
  { "dropHintString", (int)offsetof(WeaponDef, dropHintString), CSPFT_STRING },
  { "horizViewJitter", (int)offsetof(WeaponDef, horizViewJitter), CSPFT_FLOAT },
  { "vertViewJitter", (int)offsetof(WeaponDef, vertViewJitter), CSPFT_FLOAT },
  { "fightDist", (int)offsetof(WeaponDef, fightDist), CSPFT_FLOAT },
  { "maxDist", (int)offsetof(WeaponDef, maxDist), CSPFT_FLOAT },
  { "aiVsAiAccuracyGraph", (int)offsetof(WeaponDef, accuracyGraphName[0]), CSPFT_STRING },
  { "aiVsPlayerAccuracyGraph", (int)offsetof(WeaponDef, accuracyGraphName[1]), CSPFT_STRING },
  { "locNone", (int)offsetof(WeaponDef, locationDamageMultipliers[0]), CSPFT_FLOAT },
  { "locHelmet", (int)offsetof(WeaponDef, locationDamageMultipliers[1]), CSPFT_FLOAT },
  { "locHead", (int)offsetof(WeaponDef, locationDamageMultipliers[2]), CSPFT_FLOAT },
  { "locNeck", (int)offsetof(WeaponDef, locationDamageMultipliers[3]), CSPFT_FLOAT },
  { "locTorsoUpper", (int)offsetof(WeaponDef, locationDamageMultipliers[4]), CSPFT_FLOAT },
  { "locTorsoLower", (int)offsetof(WeaponDef, locationDamageMultipliers[5]), CSPFT_FLOAT },
  { "locRightArmUpper", (int)offsetof(WeaponDef, locationDamageMultipliers[6]), CSPFT_FLOAT },
  { "locRightArmLower", (int)offsetof(WeaponDef, locationDamageMultipliers[8]), CSPFT_FLOAT },
  { "locRightHand", (int)offsetof(WeaponDef, locationDamageMultipliers[10]), CSPFT_FLOAT },
  { "locLeftArmUpper", (int)offsetof(WeaponDef, locationDamageMultipliers[7]), CSPFT_FLOAT },
  { "locLeftArmLower", (int)offsetof(WeaponDef, locationDamageMultipliers[9]), CSPFT_FLOAT },
  { "locLeftHand", (int)offsetof(WeaponDef, locationDamageMultipliers[11]), CSPFT_FLOAT },
  { "locRightLegUpper", (int)offsetof(WeaponDef, locationDamageMultipliers[12]), CSPFT_FLOAT },
  { "locRightLegLower", (int)offsetof(WeaponDef, locationDamageMultipliers[14]), CSPFT_FLOAT },
  { "locRightFoot", (int)offsetof(WeaponDef, locationDamageMultipliers[16]), CSPFT_FLOAT },
  { "locLeftLegUpper", (int)offsetof(WeaponDef, locationDamageMultipliers[13]), CSPFT_FLOAT },
  { "locLeftLegLower", (int)offsetof(WeaponDef, locationDamageMultipliers[15]), CSPFT_FLOAT },
  { "locLeftFoot", (int)offsetof(WeaponDef, locationDamageMultipliers[17]), CSPFT_FLOAT },
  { "locGun", (int)offsetof(WeaponDef, locationDamageMultipliers[18]), CSPFT_FLOAT },
  { "fireRumble", (int)offsetof(WeaponDef, fireRumble), CSPFT_STRING },
  { "meleeImpactRumble", (int)offsetof(WeaponDef, meleeImpactRumble), CSPFT_STRING },
  { "adsDofStart", (int)offsetof(WeaponDef, adsDofStart), CSPFT_FLOAT },
  { "adsDofEnd", (int)offsetof(WeaponDef, adsDofEnd), CSPFT_FLOAT }
}; // idb

// const char *szWeapTypeNames[4] = { "bullet", "grenade", "projectile", "binoculars" }; // idb
const char *szWeapClassNames[10] =
{
  "rifle",
  "mg",
  "smg",
  "spread",
  "pistol",
  "grenade",
  "rocketlauncher",
  "turret",
  "non-player",
  "item"
}; // idb

char *g_playerAnimTypeNames[64];

WeaponDef bg_defaultWeaponDefs;

char *__cdecl BG_GetPlayerAnimTypeName(int32_t index)
{
    return g_playerAnimTypeNames[index];
}

void __cdecl TRACK_bg_weapons_load_obj()
{
    track_static_alloc_internal(szWeapOverlayReticleNames, 8, "szWeapOverlayReticleNames", 9);
    track_static_alloc_internal(szWeapStanceNames, 12, "szWeapStanceNames", 9);
    track_static_alloc_internal(weaponDefFields, 6024, "weaponDefFields", 9);
    track_static_alloc_internal(&bg_defaultWeaponDefs, sizeof(bg_defaultWeaponDefs), "bg_defaultWeaponDefs", 9);
    track_static_alloc_internal(penetrateTypeNames, 16, "penetrateTypeNames", 9);
    track_static_alloc_internal(szWeapTypeNames, 16, "szWeapTypeNames", 9);
    track_static_alloc_internal(szWeapClassNames, 40, "szWeapClassNames", 9);
    track_static_alloc_internal(g_playerAnimTypeNames, 256, "g_playerAnimTypeNames", 9);
    track_static_alloc_internal(szWeapInventoryTypeNames, 16, "szWeapInventoryTypeNames", 9);
}

const char *__cdecl BG_GetWeaponTypeName(weapType_t type)
{
    bcassert(type < WEAPTYPE_NUM, ARRAY_COUNT(szWeapTypeNames));

    return szWeapTypeNames[type];
}

const char *__cdecl BG_GetWeaponClassName(weapClass_t type)
{
    bcassert(type < WEAPCLASS_NUM, ARRAY_COUNT(szWeapClassNames));

    return szWeapClassNames[type];
}

const char *__cdecl BG_GetWeaponInventoryTypeName(weapInventoryType_t type)
{
    bcassert(type < WEAPINVENTORYCOUNT, ARRAY_COUNT(szWeapInventoryTypeNames));

    return szWeapInventoryTypeNames[type];
}

#ifdef KISAK_MP
void __cdecl BG_LoadWeaponStrings()
{
    uint32_t i; // [esp+0h] [ebp-4h]

    for (i = 0; i < g_playerAnimTypeNamesCount; ++i)
        BG_InitWeaponString(i, g_playerAnimTypeNames[i]);
}
#endif

void __cdecl BG_LoadPlayerAnimTypes()
{
#ifdef KISAK_MP
    char v0; // [esp+3h] [ebp-29h]
    char *v1; // [esp+8h] [ebp-24h]
    const char *v2; // [esp+Ch] [ebp-20h]
    char *buf; // [esp+20h] [ebp-Ch]
    const char *text_p; // [esp+24h] [ebp-8h] BYREF
    const char *token; // [esp+28h] [ebp-4h]

    g_playerAnimTypeNamesCount = 0;
    buf = Com_LoadRawTextFile("mp/playeranimtypes.txt");
    if (!buf)
        Com_Error(ERR_DROP, "Couldn',27h,'t load file %s", "mp/playeranimtypes.txt");
    text_p = buf;
    Com_BeginParseSession("BG_AnimParseAnimScript");
    while (1)
    {
        token = (const char *)Com_Parse(&text_p);
        if (!token || !*token)
            break;
        if (g_playerAnimTypeNamesCount >= 0x40)
            Com_Error(ERR_DROP, "Player anim type array size exceeded");
        g_playerAnimTypeNames[g_playerAnimTypeNamesCount] = (char *)Hunk_Alloc(
            strlen(token) + 1,
            "BG_LoadPlayerAnimTypes",
            9);
        v2 = token;
        v1 = g_playerAnimTypeNames[g_playerAnimTypeNamesCount];
        do
        {
            v0 = *v2;
            *v1++ = *v2++;
        } while (v0);
        ++g_playerAnimTypeNamesCount;
    }
    Com_EndParseSession();
    Com_UnloadRawTextFile(buf);
#elif KISAK_SP
    g_playerAnimTypeNamesCount = 1;
    g_playerAnimTypeNames[0] = (char*)"none";
#endif
}

void __cdecl InitWeaponDef(WeaponDef *weapDef)
{
    const cspField_t *pField; // [esp+4h] [ebp-8h]
    int iField; // [esp+8h] [ebp-4h]

    weapDef->szInternalName = "";
    iField = 0;
    pField = weaponDefFields;
    while (iField < 502)
    {
        if (pField->iFieldType == CSPFT_STRING)
            *(const char **)((char *)&weapDef->szInternalName + pField->iOffset) = "";
        ++iField;
        ++pField;
    }
}

char __cdecl G_ParseAIWeaponAccurayGraphFile(
    const char *buffer,
    const char *fileName,
    float (*knots)[2],
    int *knotCount)
{
    int v4; // eax
    long double v5; // st7
    long double v6; // st7
    int knotCountIndex; // [esp+0h] [ebp-8h]
    parseInfo_t *tokenb; // [esp+4h] [ebp-4h]
    parseInfo_t *token; // [esp+4h] [ebp-4h]
    parseInfo_t *tokena; // [esp+4h] [ebp-4h]

    iassert(buffer);
    iassert(fileName);
    iassert(knots);
    iassert(knotCount);

    Com_BeginParseSession(fileName);
    tokenb = Com_Parse(&buffer);
    v4 = atoi(tokenb->token);
    *knotCount = v4;
    knotCountIndex = 0;
    while (1)
    {
        token = Com_Parse(&buffer);
        if (!token->token[0])
            break;
        if (token->token[0] == 125)
            break;
        v5 = atof(token->token);
        (*knots)[2 * knotCountIndex] = v5;
        tokena = Com_Parse(&buffer);
        if (!tokena->token[0] || tokena->token[0] == 125)
            break;
        v6 = atof(tokena->token);
        (*knots)[2 * knotCountIndex++ + 1] = v6;
        if (knotCountIndex >= 16)
        {
            Com_PrintWarning(CON_CHANNEL_SERVER, "WARNING: \"%s\" has too many graph knots\n", fileName);
            Com_EndParseSession();
            return 0;
        }
    }
    Com_EndParseSession();
    if (knotCountIndex == *knotCount)
    {
        if ((*knots)[2 * knotCountIndex - 2] == 1.0)
        {
            return 1;
        }
        else
        {
            Com_PrintError(CON_CHANNEL_SERVER, "ERROR: \"%s\" Range must be 0.0 to 1.0\n", fileName);
            return 0;
        }
    }
    else
    {
        Com_PrintError(CON_CHANNEL_SERVER, "ERROR: \"%s\" Error in parsing an ai weapon accuracy file\n", fileName);
        return 0;
    }
}

char __cdecl G_ParseWeaponAccurayGraphInternal(
    WeaponDef *weaponDef,
    const char *dirName,
    const char *graphName,
    float (*knots)[2],
    int *knotCount)
{
    signed int v6; // [esp+10h] [ebp-205Ch]
    char string[64]; // [esp+14h] [ebp-2058h] BYREF
    char buffer[8196]; // [esp+54h] [ebp-2018h] BYREF
    const char *last; // [esp+205Ch] [ebp-10h]
    int knotCounta; // [esp+2060h] [ebp-Ch] BYREF
    int f; // [esp+2064h] [ebp-8h] BYREF
    int len; // [esp+2068h] [ebp-4h]

    last = "WEAPONACCUFILE";
    len = strlen("WEAPONACCUFILE");
    iassert(weaponDef);
    iassert(graphName);
    iassert(knots);
    iassert(knotCount);
    iassert(dirName);

    if (weaponDef->weapType && weaponDef->weapType != WEAPTYPE_PROJECTILE)
        return 1;

    if (!*graphName)
        return 1;

    snprintf(string, ARRAYSIZE(string), "accuracy/%s/%s", dirName, graphName);
    v6 = FS_FOpenFileByMode(string, &f, FS_READ);
    if (v6 >= 0)
    {
        FS_Read((uint8_t *)buffer, len, f);
        buffer[len] = 0;
        if (!strncmp(buffer, last, len))
        {
            if (v6 - len < 0x2000)
            {
                memset((uint8_t *)buffer, 0, 0x2000u);
                FS_Read((uint8_t *)buffer, v6 - len, f);
                buffer[v6 - len] = 0;
                FS_FCloseFile(f);
                knotCounta = 0;
                if (G_ParseAIWeaponAccurayGraphFile(buffer, string, knots, &knotCounta))
                {
                    *knotCount = knotCounta;
                    return 1;
                }
                else
                {
                    return 0;
                }
            }
            else
            {
                Com_PrintWarning(CON_CHANNEL_SERVER, "WARNING: \"%s\" Is too long of an ai weapon accuracy file to parse\n", string);
                FS_FCloseFile(f);
                return 0;
            }
        }
        else
        {
            Com_PrintWarning(CON_CHANNEL_SERVER, "WARNING: \"%s\" does not appear to be an ai weapon accuracy file\n", string);
            FS_FCloseFile(f);
            return 0;
        }
    }
    else
    {
        Com_PrintWarning(CON_CHANNEL_SERVER, "WARNING: Could not load ai weapon accuracy file '%s'\n", string);
        return 0;
    }
}

char __cdecl G_ParseWeaponAccurayGraphs(WeaponDef *weaponDef)
{
    uint32_t size; // [esp+4h] [ebp-8Ch]
    int weaponType; // [esp+8h] [ebp-88h]
    int accuracyGraphKnotCount; // [esp+Ch] [ebp-84h] BYREF
    float accuracyGraphKnots[16][2]; // [esp+10h] [ebp-80h] BYREF

    for (weaponType = 0; weaponType < 2; ++weaponType)
    {
        memset((uint8_t *)accuracyGraphKnots, 0, sizeof(accuracyGraphKnots));
        accuracyGraphKnotCount = 0;
        if (!G_ParseWeaponAccurayGraphInternal(
            weaponDef,
            accuracyDirName[weaponType],
            weaponDef->accuracyGraphName[weaponType],
            accuracyGraphKnots,
            &accuracyGraphKnotCount))
            return 0;
        if (accuracyGraphKnotCount > 0)
        {
            size = 8 * accuracyGraphKnotCount;
            weaponDef->accuracyGraphKnots[weaponType] = (float (*)[2])Hunk_AllocLowAlign(
                8 * accuracyGraphKnotCount,
                4,
                "G_ParseWeaponAccurayGraphs",
                9);
            weaponDef->originalAccuracyGraphKnots[weaponType] = weaponDef->accuracyGraphKnots[weaponType];
            memcpy((uint8_t *)weaponDef->accuracyGraphKnots[weaponType], (uint8_t *)accuracyGraphKnots, size);
            weaponDef->accuracyGraphKnotCount[weaponType] = accuracyGraphKnotCount;
            weaponDef->originalAccuracyGraphKnotCount[weaponType] = weaponDef->accuracyGraphKnotCount[weaponType];
        }
    }
    return 1;
}

WeaponDef *__cdecl BG_LoadDefaultWeaponDef_LoadObj()
{
    InitWeaponDef(&bg_defaultWeaponDefs);
    bg_defaultWeaponDefs.szInternalName = "none";
    bg_defaultWeaponDefs.accuracyGraphName[0] = "noweapon.accu";
    bg_defaultWeaponDefs.accuracyGraphName[1] = "noweapon.accu";
    bg_defaultWeaponDefs.sprintDurationScale = 1.75;
    G_ParseWeaponAccurayGraphs(&bg_defaultWeaponDefs);
    return &bg_defaultWeaponDefs;
}

WeaponDef *__cdecl BG_LoadDefaultWeaponDef()
{
    if (IsFastFileLoad())
        return BG_LoadDefaultWeaponDef_FastFile();
    else
        return BG_LoadDefaultWeaponDef_LoadObj();
}

WeaponDef *__cdecl BG_LoadDefaultWeaponDef_FastFile()
{
    return DB_FindXAssetHeader(ASSET_TYPE_WEAPON, "none").weapon;
}

int __cdecl Weapon_GetStringArrayIndex(const char *value, char **stringArray, int arraySize)
{
    int arrayIndex; // [esp+0h] [ebp-4h]

    iassert(value);
    iassert(stringArray);

    for (arrayIndex = 0; arrayIndex < arraySize; ++arrayIndex)
    {
        if (!I_stricmp(value, stringArray[arrayIndex]))
            return arrayIndex;
    }
    return -1;
}

snd_alias_list_t **__cdecl BG_RegisterSurfaceTypeSounds(const char *surfaceSoundBase)
{
    char *v2; // eax
    snd_alias_list_t *SoundAlias; // eax
    char v4; // [esp+3h] [ebp-131h]
    char *v5; // [esp+8h] [ebp-12Ch]
    const char *v6; // [esp+Ch] [ebp-128h]
    snd_alias_list_t **result; // [esp+20h] [ebp-114h]
    char aliasName[260]; // [esp+24h] [ebp-110h] BYREF
    snd_alias_list_t *defaultAliasList; // [esp+12Ch] [ebp-8h]
    int i; // [esp+130h] [ebp-4h]

    iassert(surfaceSoundBase);

    if (!*surfaceSoundBase)
        return 0;

    for (i = 0; i < surfaceTypeSoundListCount; ++i)
    {
        if (!I_strcmp(surfaceTypeSoundLists[i].surfaceSoundBase, surfaceSoundBase))
            return surfaceTypeSoundLists[i].soundAliasList;
    }
    if (surfaceTypeSoundListCount == 16)
        Com_Error(ERR_DROP, "Exceeded MAX_SURFACE_TYPE_SOUND_LISTS (%d)", 16);

    // 29 pointers, sized from the pointer type: the original 0x74 assumed 4-byte pointers and the
    // loop below would run off the end of the allocation.
    result = (snd_alias_list_t **)Hunk_AllocLow(sizeof(snd_alias_list_t *) * 29, "BG_RegisterSurfaceTypeSounds", 15);
    Com_sprintf(aliasName, 0x100u, "%s_default", surfaceSoundBase);
    defaultAliasList = Com_FindSoundAlias(aliasName);
    for (i = 0; i < 29; ++i)
    {
        v2 = (char*)Com_SurfaceTypeToName(i);
        Com_sprintf(aliasName, 0x100u, "%s_%s", surfaceSoundBase, v2);
        SoundAlias = Com_FindSoundAlias(aliasName);
        result[i] = SoundAlias;
        if (!result[i])
            result[i] = defaultAliasList;
    }
    surfaceTypeSoundLists[surfaceTypeSoundListCount].surfaceSoundBase = (char *)Hunk_AllocLow(
        strlen(surfaceSoundBase) + 1,
        "BG_RegisterSurfaceTypeSounds",
        15);
    v6 = surfaceSoundBase;
    v5 = surfaceTypeSoundLists[surfaceTypeSoundListCount].surfaceSoundBase;
    do
    {
        v4 = *v6;
        *v5++ = *v6++;
    } while (v4);
    surfaceTypeSoundLists[surfaceTypeSoundListCount++].soundAliasList = result;
    return result;
}

int __cdecl BG_ParseWeaponDefSpecificFieldType(uint8_t *pStruct, const char *pValue, int iFieldType)
{
    uint16_t LowercaseString_DONE; // ax
    uint16_t v5; // ax
    int result; // eax
    char v7; // [esp+3h] [ebp-91h]
    char *v8; // [esp+8h] [ebp-8Ch]
    const char *v9; // [esp+Ch] [ebp-88h]
    int v10; // [esp+10h] [ebp-84h]
    const char *pos; // [esp+38h] [ebp-5Ch] BYREF
    int numHideTags; // [esp+3Ch] [ebp-58h]
    int numNoteTrackMappings; // [esp+40h] [ebp-54h]
    char keyName[64]; // [esp+44h] [ebp-50h] BYREF
    int arrayIndex; // [esp+88h] [ebp-Ch]
    const char *token; // [esp+8Ch] [ebp-8h]
    WeaponDef *weapDef; // [esp+90h] [ebp-4h]

    iassert(pStruct);
    iassert(pValue);

    weapDef = (WeaponDef *)pStruct;
    switch (iFieldType)
    {
    case WFT_WEAPONTYPE:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char**)szWeapTypeNames, 4);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon type %s in %s", pValue, weapDef->szInternalName);
        weapDef->weapType = (weapType_t)arrayIndex;
        goto LABEL_86;
    case WFT_WEAPONCLASS:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char**)szWeapClassNames, 10);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon class %s in %s", pValue, weapDef->szInternalName);
        weapDef->weapClass = (weapClass_t)arrayIndex;
        goto LABEL_86;
    case WFT_OVERLAYRETICLE:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)szWeapOverlayReticleNames, 2);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon reticle %s in %s", pValue, weapDef->szInternalName);
        weapDef->overlayReticle = (weapOverlayReticle_t)arrayIndex;
        goto LABEL_86;
    case WFT_PENETRATE_TYPE:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)penetrateTypeNames, 4);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon penetrate type %s in %s", pValue, weapDef->szInternalName);
        weapDef->penetrateType = (PenetrateType)arrayIndex;
        goto LABEL_86;
    case WFT_IMPACT_TYPE:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)impactTypeNames, 9);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon impact type %s in %s", pValue, weapDef->szInternalName);
        weapDef->impactType = (ImpactType)arrayIndex;
        goto LABEL_86;
    case WFT_STANCE:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)szWeapStanceNames, 3);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon stance %s in %s", pValue, weapDef->szInternalName);
        weapDef->stance = (weapStance_t)arrayIndex;
        goto LABEL_86;
    case WFT_PROJ_EXPLOSION:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)szProjectileExplosionNames, 7);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon projExplosion %s in %s", pValue, weapDef->szInternalName);
        weapDef->projExplosion = (weapProjExposion_t)arrayIndex;
        goto LABEL_86;
    case WFT_OFFHAND_CLASS:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)offhandClassNames, 4);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon offhand class %s in %s", pValue, weapDef->szInternalName);
        weapDef->offhandClass = (OffhandClass)arrayIndex;
        goto LABEL_86;
    case WFT_ANIMTYPE:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, g_playerAnimTypeNames, g_playerAnimTypeNamesCount);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon player anim type %s in %s", pValue, weapDef->szInternalName);
        weapDef->playerAnimType = arrayIndex;
        goto LABEL_86;
    case WFT_ACTIVE_RETICLE_TYPE:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)activeReticleNames, 3);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon active reticle type %s in %s", pValue, weapDef->szInternalName);
        weapDef->activeReticleType = (activeReticleType_t)arrayIndex;
        goto LABEL_86;
    case WFT_GUIDED_MISSILE_TYPE:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)guidedMissileNames, 4);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon guided missile type %s in %s", pValue, weapDef->szInternalName);
        weapDef->guidedMissileType = (guidedMissileType_t)arrayIndex;
        goto LABEL_86;
    case WFT_BOUNCE_SOUND:
        weapDef->bounceSound = BG_RegisterSurfaceTypeSounds(pValue);
        goto LABEL_86;
    case WFT_STICKINESS:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)stickinessNames, 4);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon stickiness %s in %s", pValue, weapDef->szInternalName);
        weapDef->stickiness = (WeapStickinessType)arrayIndex;
        goto LABEL_86;
    case WFT_OVERLAYINTERFACE:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)overlayInterfaceNames, 3);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon overlay interface %s in %s", pValue, weapDef->szInternalName);
        weapDef->overlayInterface = (WeapOverlayInteface_t)arrayIndex;
        goto LABEL_86;
    case WFT_INVENTORYTYPE:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)szWeapInventoryTypeNames, 4);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon inventory type %s in %s", pValue, weapDef->szInternalName);
        weapDef->inventoryType = (weapInventoryType_t)arrayIndex;
        goto LABEL_86;
    case WFT_FIRETYPE:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)szWeapFireTypeNames, 5);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon firetype %s in %s", pValue, weapDef->szInternalName);
        weapDef->fireType = (weapFireType_t)arrayIndex;
        goto LABEL_86;
    case WFT_AMMOCOUNTER_CLIPTYPE:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char**)ammoCounterClipNames, 7);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon ammo counter clip %s in %s", pValue, weapDef->szInternalName);
        weapDef->ammoCounterClip = (ammoCounterClipType_t)arrayIndex;
        goto LABEL_86;
    case WFT_ICONRATIO_HUD:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)weapIconRatioNames, 3);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon hud icon ratio %s in %s", pValue, weapDef->szInternalName);
        weapDef->hudIconRatio = (weaponIconRatioType_t)arrayIndex;
        goto LABEL_86;
    case WFT_ICONRATIO_AMMOCOUNTER:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)weapIconRatioNames, 3);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon ammo counter icon ratio %s in %s", pValue, weapDef->szInternalName);
        weapDef->ammoCounterIconRatio = (weaponIconRatioType_t)arrayIndex;
        goto LABEL_86;
    case WFT_ICONRATIO_KILL:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)weapIconRatioNames, 3);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon kill icon ratio %s in %s", pValue, weapDef->szInternalName);
        weapDef->killIconRatio = (weaponIconRatioType_t)arrayIndex;
        goto LABEL_86;
    case WFT_ICONRATIO_DPAD:
        arrayIndex = Weapon_GetStringArrayIndex(pValue, (char **)weapIconRatioNames, 3);
        if (arrayIndex < 0)
            Com_Error(ERR_DROP, "Unknown weapon dpad icon ratio %s in %s", pValue, weapDef->szInternalName);
        weapDef->dpadIconRatio = (weaponIconRatioType_t)arrayIndex;
        goto LABEL_86;
    case WFT_HIDETAGS:
        numHideTags = 0;
        pos = pValue;
        while (1)
        {
            token = (const char *)Com_Parse(&pos);
            if (!pos)
                break;
            if (numHideTags >= 8)
                Com_Error(ERR_DROP, "maximum hide tags (%s) exceeded: %i > %i'", token, numHideTags, 8);
            weapDef->hideTags[numHideTags] = SL_GetStringOfSize((char *)token, 0, strlen(token) + 1, MT_TYPE_MODEL_PART);
            weapDef->hideTags[numHideTags] = SL_ConvertToLowercase(weapDef->hideTags[numHideTags], 0, MT_TYPE_MODEL_PART);
            ++numHideTags;
        }
        goto LABEL_86;
    case WFT_NOTETRACKSOUNDMAP:
        numNoteTrackMappings = 0;
        pos = pValue;
        keyName[0] = 0;
        while (1)
        {
            token = (const char *)Com_Parse(&pos);
            if (!pos)
                break;
            if (numNoteTrackMappings >= 16)
                Com_Error(ERR_DROP, "Max notetrack-to-sound mappings (%i) exceeded with entry '%s'", 16, token);
            if (keyName[0])
            {
                LowercaseString_DONE = SL_GetLowercaseString(keyName, 0);
                weapDef->notetrackSoundMapKeys[numNoteTrackMappings] = LowercaseString_DONE;
                v5 = SL_GetLowercaseString(token, 0);
                weapDef->notetrackSoundMapValues[numNoteTrackMappings++] = v5;
                keyName[0] = 0;
            }   
            else
            {
                v10 = strlen(token);
                if (v10 >= 63)
                    Com_Error(ERR_DROP, "Notetrack - to - sound: keyname \"%s\" is too long(length % i / % i).", token, v10, 63);
                v9 = token;
                v8 = keyName;
                do
                {
                    v7 = *v9;
                    *v8++ = *v9++;
                } while (v7);
            }
        }
        if (keyName[0])
            Com_PrintWarning(
                CON_CHANNEL_DONT_FILTER,
                "Notetrack-to-Sound: Weapon '%s' has bad entry; notetrack '%s' doesn't have a corresponding sound.\n",
                weapDef->szInternalName,
                keyName);
    LABEL_86:
        result = 1;
        break;
    default:
        Com_Error(ERR_DROP, "Bad field type %i in %s", iFieldType, weapDef->szInternalName);
        result = 0;
        break;
    }
    return result;
}

void __cdecl BG_SetupTransitionTimes(WeaponDef *weapDef)
{
    double v1; // st7
    double v2; // st7

    if (weapDef->iAdsTransInTime <= 0)
        v1 = 1.0 / (float)300.0;
    else
        v1 = 1.0 / (double)weapDef->iAdsTransInTime;
    weapDef->fOOPosAnimLength[0] = v1;
    if (weapDef->iAdsTransOutTime <= 0)
        v2 = 1.0 / (float)500.0;
    else
        v2 = 1.0 / (double)weapDef->iAdsTransOutTime;
    weapDef->fOOPosAnimLength[1] = v2;
}

void __cdecl BG_CheckWeaponDamageRanges(WeaponDef *weapDef)
{
    if (weapDef->fMaxDamageRange <= 0.0f)
        weapDef->fMaxDamageRange = 999999.0f;
    if (weapDef->fMinDamageRange <= 0.0f)
        weapDef->fMinDamageRange = 999999.12f;
}

void __cdecl BG_CheckProjectileValues(WeaponDef *weaponDef)
{
    iassert(weaponDef->weapType == WEAPTYPE_PROJECTILE);

    if ((double)weaponDef->iProjectileSpeed <= 0.0)
        Com_Error(ERR_DROP, "Projectile speed for WeapType %s must be greater than 0.0", weaponDef->szDisplayName);

    if (weaponDef->destabilizationCurvatureMax >= 1000000000.0f || weaponDef->destabilizationCurvatureMax < 0.0)
        Com_Error(
            ERR_DROP,
            "Destabilization angle for for WeapType %s must be between 0 and 45 degrees",
            weaponDef->szDisplayName);

    if (weaponDef->destabilizationRateTime < 0.0)
        Com_Error(ERR_DROP, "Destabilization rate time for for WeapType %s must be non-negative", weaponDef->szDisplayName);
}

WeaponDef *__cdecl BG_LoadWeaponDefInternal(const char *one, const char *two)
{
    snd_alias_list_t *SoundAlias; // eax
    snd_alias_list_t *v4; // eax
    snd_alias_list_t *v5; // eax
    snd_alias_list_t *v6; // eax
    snd_alias_list_t *v7; // eax
    char buffer[10244]; // [esp+1Ch] [ebp-2858h] BYREF
    int f; // [esp+2820h] [ebp-54h] BYREF
    int len; // [esp+2824h] [ebp-50h]
    signed int v11; // [esp+2828h] [ebp-4Ch]
    char dest[64]; // [esp+282Ch] [ebp-48h] BYREF
    WeaponDef *weapDef; // [esp+2870h] [ebp-4h]

    len = strlen("WEAPONFILE");
    // sizeof, not the 32-bit 0x878: the struct is larger once its pointers are 64-bit, and the
    // parser was writing the tail of every weapon past the end of this allocation.
    weapDef = (WeaponDef *)Hunk_AllocLow(sizeof(WeaponDef), "BG_LoadWeaponDefInternal", 9);
    InitWeaponDef(weapDef);
    Com_sprintf(dest, 0x40u, "weapons/%s/%s", one, two);
    v11 = FS_FOpenFileByMode(dest, &f, FS_READ);
    if (v11 >= 0)
    {
        FS_Read((uint8_t *)buffer, len, f);
        buffer[len] = 0;
        if (!strncmp(buffer, "WEAPONFILE", len))
        {
            if ((uint32_t)(v11 - len) < 0x2800)
            {
                memset((uint8_t *)buffer, 0, 0x2800u);
                FS_Read((uint8_t *)buffer, v11 - len, f);
                buffer[v11 - len] = 0;
                FS_FCloseFile(f);
                if (Info_Validate(buffer))
                {
                    SetConfigString((char **)weapDef, two);
                    if (ParseConfigStringToStructCustomSize(
                        (uint8_t *)weapDef,
                        weaponDefFields,
                        502,
                        buffer,
                        WFT_NUM_FIELD_TYPES,
                        BG_ParseWeaponDefSpecificFieldType,
                        SetConfigString2))
                    {
                        if (I_stricmp(two, "defaultweapon_mp"))
                        {
                            if (!weapDef->viewLastShotEjectEffect)
                                weapDef->viewLastShotEjectEffect = weapDef->viewShellEjectEffect;
                            if (!weapDef->worldLastShotEjectEffect)
                                weapDef->worldLastShotEjectEffect = weapDef->worldShellEjectEffect;
                            if (!weapDef->raiseSound)
                            {
                                SoundAlias = Com_FindSoundAlias("weap_raise");
                                weapDef->raiseSound = SoundAlias;
                            }
                            if (!weapDef->putawaySound)
                            {
                                v4 = Com_FindSoundAlias("weap_putaway");
                                weapDef->putawaySound = v4;
                            }
                            if (!weapDef->pickupSound)
                            {
                                v5 = Com_FindSoundAlias("weap_pickup");
                                weapDef->pickupSound = v5;
                            }
                            if (!weapDef->ammoPickupSound)
                            {
                                v6 = Com_FindSoundAlias("weap_ammo_pickup");
                                weapDef->ammoPickupSound = v6;
                            }
                            if (!weapDef->emptyFireSound)
                            {
                                v7 = Com_FindSoundAlias("weap_dryfire_smg_npc");
                                weapDef->emptyFireSound = v7;
                            }
                        }
                        BG_SetupTransitionTimes(weapDef);
                        BG_CheckWeaponDamageRanges(weapDef);
                        if (weapDef->enemyCrosshairRange > 15000.0)
                            Com_Error(ERR_DROP, "Enemy crosshair ranges should be less than %f ", 15000.0);
                        if (weapDef->weapType == WEAPTYPE_PROJECTILE)
                            BG_CheckProjectileValues(weapDef);
                        if (G_ParseWeaponAccurayGraphs(weapDef))
                        {
                            I_strlwr((char *)weapDef->szAmmoName);
                            I_strlwr((char *)weapDef->szClipName);
                            return weapDef;
                        }
                        else
                        {
                            return 0;
                        }
                    }
                    else
                    {
                        return 0;
                    }
                }
                else
                {
                    Com_PrintWarning(CON_CHANNEL_PLAYERWEAP, "WARNING: \"%s\" is not a valid weapon file\n", dest);
                    return 0;
                }
            }
            else
            {
                Com_PrintWarning(
                    CON_CHANNEL_PLAYERWEAP,
                    "WARNING: \"%s\" Is too long of a weapon file to parse (fileLength = %d identifierLength = %d)\n",
                    dest,
                    v11,
                    len);
                FS_FCloseFile(f);
                return 0;
            }
        }
        else
        {
            Com_PrintWarning(CON_CHANNEL_PLAYERWEAP, "WARNING: \"%s\" does not appear to be a weapon file\n", dest);
            FS_FCloseFile(f);
            return 0;
        }
    }
    else
    {
        Com_PrintWarning(CON_CHANNEL_PLAYERWEAP, "WARNING: Could not load weapon file '%s'\n", dest);
        return 0;
    }
}


WeaponDef *__cdecl BG_LoadWeaponDef_LoadObj(const char *name)
{
    WeaponDef *weapDef; // [esp+0h] [ebp-4h]

    if (!*name)
        return 0;
#ifdef KISAK_MP
    weapDef = BG_LoadWeaponDefInternal("mp", name);
#elif KISAK_SP
    weapDef = BG_LoadWeaponDefInternal("sp", name);
#endif
    if (weapDef)
        return weapDef;

#ifdef KISAK_MP
    weapDef = BG_LoadWeaponDefInternal("mp", "defaultweapon_mp");
#elif KISAK_SP
    weapDef = BG_LoadWeaponDefInternal("sp", "defaultweapon");
#endif

    if (!weapDef)
        Com_Error(ERR_DROP, "BG_LoadWeaponDef: Could not find default weapon");

    SetConfigString((char **)weapDef, name);
    return weapDef;
}
