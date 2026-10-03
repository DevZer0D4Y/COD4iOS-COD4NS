#include <cstddef>
#ifndef KISAK_SP 
#error This file is for SinglePlayer only 
#endif

#include <universal/q_shared.h>
#include "sentient_fields.h"
#include <script/scr_vm.h>
#include "g_local.h"

static const sentient_fields_s fields_2[14] =
{
  { "team", offsetof(sentient_s, eTeam), F_INT, SentientScr_SetTeam, SentientScr_GetTeam },
  { "threatbias", offsetof(sentient_s, iThreatBias), F_INT, NULL, NULL },
  { "threatbiasgroup", offsetof(sentient_s, iThreatBiasGroupIndex), F_INT, SentientScr_ReadOnly, NULL },
  { "node", offsetof(sentient_s, pClaimedNode), F_PATHNODE, SentientScr_ReadOnly, NULL },
  { "prevnode", offsetof(sentient_s, pPrevClaimedNode), F_PATHNODE, SentientScr_ReadOnly, NULL },
  { "enemy", offsetof(sentient_s, targetEnt), F_ENTHANDLE, SentientScr_ReadOnly, NULL },
  { "syncedmeleetarget", offsetof(sentient_s, syncedMeleeEnt), F_ENTHANDLE, NULL, NULL },
  { "ignoreme", offsetof(sentient_s, bIgnoreMe), F_BYTE, NULL, NULL },
  { "ignoreall", offsetof(sentient_s, bIgnoreAll), F_BYTE, NULL, NULL },
  { "maxvisibledist", offsetof(sentient_s, maxVisibleDist), F_FLOAT, NULL, NULL },
  { "attackeraccuracy", offsetof(sentient_s, attackerAccuracy), F_FLOAT, NULL, NULL },
  { "ignorerandombulletdamage", offsetof(sentient_s, ignoreRandomBulletDamage), F_BYTE, NULL, NULL },
  { "turretinvulnerability", offsetof(sentient_s, turretInvulnerability), F_BYTE, NULL, NULL },
  { NULL, 0, F_INT, NULL, NULL }
};



void __cdecl SentientScr_ReadOnly(sentient_s *pSelf, const sentient_fields_s *pField)
{
    const char *v3; // r3

    if (!pSelf)
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\game\\sentient_fields.cpp", 48, 0, "%s", "pSelf");
    v3 = va("sentient property '%s' is read-only", pField->name);
    Scr_Error(v3);
}

void __cdecl SentientScr_SetTeam(sentient_s *pSelf, const sentient_fields_s *pField)
{
    const char *String; // r31

    iassert(pSelf);

    String = Scr_GetString(0);
    if (I_stricmp(String, "axis"))
    {
        if (I_stricmp(String, "allies"))
        {
            if (I_stricmp(String, "neutral"))
            {
                Scr_Error(va("unknown team '%s', should be axis, allies, or neutral\n", String));
            }
            else
            {
                Sentient_SetTeam(pSelf, TEAM_NEUTRAL);
            }
        }
        else
        {
            Sentient_SetTeam(pSelf, TEAM_ALLIES);
        }
    }
    else
    {
        Sentient_SetTeam(pSelf, TEAM_AXIS);
    }
}

void __cdecl SentientScr_GetTeam(sentient_s *pSelf, const sentient_fields_s *pField)
{
    team_t eTeam; // r11

    iassert(pSelf);

    eTeam = pSelf->eTeam;
    if (eTeam <= TEAM_FREE || eTeam >= TEAM_NUM_TEAMS)
        MyAssertHandler(
            "c:\\trees\\cod3\\cod3src\\src\\game\\sentient_fields.cpp",
            83,
            0,
            "%s",
            "pSelf->eTeam > TEAM_BAD && pSelf->eTeam < TEAM_NUM_TEAMS");
    switch (pSelf->eTeam)
    {
    case TEAM_AXIS:
        Scr_AddString("axis");
        break;
    case TEAM_ALLIES:
        Scr_AddString("allies");
        break;
    case TEAM_NEUTRAL:
        Scr_AddString("neutral");
        break;
    case TEAM_DEAD:
        Scr_AddString("dead");
        break;
    default:
        if (!alwaysfails)
            MyAssertHandler(
                "c:\\trees\\cod3\\cod3src\\src\\game\\sentient_fields.cpp",
                100,
                0,
                "SentientScr_GetTeam: default case (shouldn't happen");
        break;
    }
}

void __cdecl GScr_AddFieldsForSentient()
{
    const sentient_fields_s *f; // [esp+4h] [ebp-4h]

    for (f = fields_2; f->name; ++f)
    {
        iassert(!((f - fields_2) & ENTFIELD_MASK));
        iassert((f - fields_2) == (unsigned short)(f - fields_2));

        Scr_AddClassField(CLASS_NUM_ENTITY, (char*)f->name, (unsigned __int16)(f - fields_2) | ENTFIELD_SENTIENT);
    }

}

void __cdecl Scr_SetSentientField(sentient_s *sentient, unsigned int offset)
{
    const sentient_fields_s *v4; // r4
    void(__cdecl * setter)(sentient_s *, const sentient_fields_s *); // r11

    if (!sentient)
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\game\\sentient_fields.cpp", 135, 0, "%s", "sentient");
    if (offset >= 0xD)
        MyAssertHandler(
            "c:\\trees\\cod3\\cod3src\\src\\game\\sentient_fields.cpp",
            136,
            0,
            "%s",
            "(unsigned)offset < ARRAY_COUNT( fields ) - 1");
    v4 = &fields_2[offset];
    setter = v4->setter;
    if (setter)
        setter(sentient, v4);
    else
        Scr_SetGenericField((unsigned __int8 *)sentient, v4->type, v4->ofs);
}

void __cdecl Scr_GetSentientField(sentient_s *sentient, unsigned int offset)
{
    const sentient_fields_s *v4; // r4
    void(__cdecl * getter)(sentient_s *, const sentient_fields_s *); // r11

    if (!sentient)
        MyAssertHandler("c:\\trees\\cod3\\cod3src\\src\\game\\sentient_fields.cpp", 159, 0, "%s", "sentient");
    if (offset >= 0xD)
        MyAssertHandler(
            "c:\\trees\\cod3\\cod3src\\src\\game\\sentient_fields.cpp",
            160,
            0,
            "%s",
            "(unsigned)offset < ARRAY_COUNT( fields ) - 1");
    v4 = &fields_2[offset];
    getter = v4->getter;
    if (getter)
        getter(sentient, v4);
    else
        Scr_GetGenericField((unsigned __int8 *)sentient, v4->type, v4->ofs);
}

