#ifndef KISAK_MP
#error This File is MultiPlayer Only
#endif

#include <universal/q_shared.h>
#include <client_mp/cl_cod4x.h>
#include "client_mp.h"
#include <qcommon/mem_track.h>
#include <qcommon/msg_mp.h>
#include <cgame_mp/cg_local_mp.h>
#include <universal/com_files.h>
#include <win32/win_local.h>
#include <qcommon/cmd.h>
#include <qcommon/dl_main.h>
#include <universal/com_constantconfigstrings.h>
#include <database/database.h>
#include <qcommon/files.h>
#include <client/client.h>
#include <server_mp/server_mp.h>
void MSG_TraceDump(const char *why);
#include <ui_mp/ui_mp.h>
#include <stringed/stringed_hooks.h>

const char* svc_strings[256] = {
    "svc_nop",
    "svc_gamestate",
    "svc_configstring",
    "svc_baseline",
    "svc_serverCommand",
    "svc_download",
    "svc_snapshot",
    "svc_EOF"
};
int autoupdateStarted;
char autoupdateFilename[64];
int cl_connectedToPureServer;

constexpr size_t CLIENT_ARCHIVE_SIZE = 256;

void __cdecl TRACK_cl_parse()
{
    track_static_alloc_internal(svc_strings, 1024, "svc_strings", 9);
}

void __cdecl SHOWNET(msg_t *msg, const char *s)
{
    if (cl_shownet->current.integer >= 2)
        Com_Printf(CON_CHANNEL_CLIENT, "%3i:%s\n", msg->readcount - 1, s);
}

void __cdecl CL_SavePredictedOriginForServerTime(
    clientActive_t *cl,
    int serverTime,
    float *predictedOrigin,
    float *predictedVelocity,
    float *viewangles,
    int bobCycle,
    int movementDir)
{
    float *v7; // [esp+0h] [ebp-10h]
    float *velocity; // [esp+4h] [ebp-Ch]
    float *origin; // [esp+8h] [ebp-8h]
    uint32_t lastIndex; // [esp+Ch] [ebp-4h]

    lastIndex = (cl->clientArchiveIndex + CLIENT_ARCHIVE_SIZE - 1) % CLIENT_ARCHIVE_SIZE;
    if (lastIndex >= CLIENT_ARCHIVE_SIZE)
        MyAssertHandler(
            ".\\client_mp\\cl_parse_mp.cpp",
            80,
            0,
            "lastIndex doesn't index CLIENT_ARCHIVE_SIZE\n\t%i not in [0, %i)",
            lastIndex,
            CLIENT_ARCHIVE_SIZE);
    if (cl->clientArchive[lastIndex].serverTime != serverTime)
    {
        cl->clientArchive[cl->clientArchiveIndex].serverTime = serverTime;
        origin = cl->clientArchive[cl->clientArchiveIndex].origin;
        *origin = *predictedOrigin;
        origin[1] = predictedOrigin[1];
        origin[2] = predictedOrigin[2];
        velocity = cl->clientArchive[cl->clientArchiveIndex].velocity;
        *velocity = *predictedVelocity;
        velocity[1] = predictedVelocity[1];
        velocity[2] = predictedVelocity[2];
        cl->clientArchive[cl->clientArchiveIndex].bobCycle = bobCycle;
        cl->clientArchive[cl->clientArchiveIndex].movementDir = movementDir;
        v7 = cl->clientArchive[cl->clientArchiveIndex].viewangles;
        *v7 = *viewangles;
        v7[1] = viewangles[1];
        v7[2] = viewangles[2];
        cl->clientArchiveIndex = (cl->clientArchiveIndex + 1) % CLIENT_ARCHIVE_SIZE;
        if (cl->clientArchiveIndex >= CLIENT_ARCHIVE_SIZE)
            MyAssertHandler(
                ".\\client_mp\\cl_parse_mp.cpp",
                92,
                0,
                "cl->clientArchiveIndex doesn't index CLIENT_ARCHIVE_SIZE\n\t%i not in [0, %i)",
                cl->clientArchiveIndex,
                CLIENT_ARCHIVE_SIZE);
    }
}

bool __cdecl CL_GetPredictedOriginForServerTime(
    clientActive_t *cl,
    int serverTime,
    float *predictedOrigin,
    float *predictedVelocity,
    float *viewangles,
    int *bobCycle,
    int *movementDir)
{
    for (int cmd = 0; cmd < CLIENT_ARCHIVE_SIZE; ++cmd)
    {
        int index = (cl->clientArchiveIndex + CLIENT_ARCHIVE_SIZE - cmd - 1) % CLIENT_ARCHIVE_SIZE;
        bcassert(index, CLIENT_ARCHIVE_SIZE);
        if (cl->clientArchive[index].serverTime <= serverTime)
        {
            if (cl->clientArchive[index].serverTime != serverTime)
            {
                Com_Printf(CON_CHANNEL_CLIENT, "Couldn't find exact match for servertime %i, using servertime %i\n", serverTime, cl->clientArchive[index].serverTime);
            }
                
            predictedOrigin[0] = cl->clientArchive[index].origin[0];
            predictedOrigin[1] = cl->clientArchive[index].origin[1];
            predictedOrigin[2] = cl->clientArchive[index].origin[2];
            predictedVelocity[0] = cl->clientArchive[index].velocity[0];
            predictedVelocity[1] = cl->clientArchive[index].velocity[1];
            predictedVelocity[2] = cl->clientArchive[index].velocity[2];
            viewangles[0] = cl->clientArchive[index].viewangles[0];
            viewangles[1] = cl->clientArchive[index].viewangles[1];
            viewangles[2] = cl->clientArchive[index].viewangles[2];
            *bobCycle = cl->clientArchive[index].bobCycle;
            *movementDir = cl->clientArchive[index].movementDir;
            iassert(!IS_NAN((predictedOrigin)[0]) && !IS_NAN((predictedOrigin)[1]) && !IS_NAN((predictedOrigin)[2]));
            iassert(!IS_NAN((predictedVelocity)[0]) && !IS_NAN((predictedVelocity)[1]) && !IS_NAN((predictedVelocity)[2]));

            return true;
        }
    }

    Com_PrintError(CON_CHANNEL_CLIENT, "Unable to find predicted origin for server time %i.  Here's what we have:\n", serverTime);

    for (int cmd = 0; cmd < CLIENT_ARCHIVE_SIZE; ++cmd)
    {
        int index = (cl->clientArchiveIndex + CLIENT_ARCHIVE_SIZE - cmd - 1) % CLIENT_ARCHIVE_SIZE;
        bcassert(index, CLIENT_ARCHIVE_SIZE);
        Com_PrintError(CON_CHANNEL_CLIENT, "%i: %i\n", index, cl->clientArchive[index].serverTime);
    }

    return false;
}

void __cdecl CL_DeltaClient(
    clientActive_t *cl,
    msg_t *msg,
    int time,
    clSnapshot_t *frame,
    uint32_t newnum,
    const clientState_s *old,
    int unchanged)
{
    clientState_s *state; // [esp+8h] [ebp-4h]

    state = &cl->parseClients[cl->parseClientsNum & (MAX_PARSE_CLIENTS - 1)];
    if (unchanged)
    {
        memcpy(state, old, sizeof(clientState_s));
    }
    else if (MSG_ReadDeltaClient(msg, time, old, state, newnum))
    {
        return;
    }
    ++cl->parseClientsNum;
    ++frame->numClients;
}

void __cdecl CL_SystemInfoChanged(int localClientNum)
{
    const char *v1; // eax
    const char *v2; // eax
    const char *v3; // eax
    clientActive_t *LocalClientGlobals; // [esp+4h] [ebp-28h]
    const char *t; // [esp+8h] [ebp-24h]
    char *systemInfo; // [esp+14h] [ebp-18h]
    char (*key)[8192]; // [esp+18h] [ebp-14h]
    const char *s; // [esp+24h] [ebp-8h] BYREF
    char (*value)[8192]; // [esp+28h] [ebp-4h]

    LargeLocal key_large_local(0x2000); // [esp+1Ch] [ebp-10h] BYREF
    //LargeLocal::LargeLocal(&key_large_local, 0x2000);
    //key = (char (*)[8192])LargeLocal::GetBuf(&key_large_local);
    key = (char (*)[8192])key_large_local.GetBuf();

    LargeLocal value_large_local(0x2000); // [esp+Ch] [ebp-20h] BYREF
    //LargeLocal::LargeLocal(&value_large_local, 0x2000);
    //value = (char (*)[8192])LargeLocal::GetBuf(&value_large_local);
    value = (char (*)[8192])value_large_local.GetBuf();

    LocalClientGlobals = CL_GetLocalClientGlobals(localClientNum);
    systemInfo = &LocalClientGlobals->gameState.stringData[LocalClientGlobals->gameState.stringOffsets[1]];
    v1 = Info_ValueForKey(systemInfo, "sv_serverid");
    LocalClientGlobals->serverId = atoi(v1);
    if (!CL_GetLocalClientConnection(localClientNum)->demoplaying)
    {
        // CoD4X servers hand a protocol-6 client a fixed placeholder gamestate instead of the real
        // one: always mp_shipment, 256 slots, host "^5CoD4Host", with a map-start date months in
        // the past. It is byte-identical from unrelated servers. Nothing can be played from it -
        // no snapshot ever arrives - so the map load and the cl_timeout wait that follow are pure
        // dead time. Recognise it and say why instead.
        const char *serverInfo = CL_GetConfigString(localClientNum, 0);
        char host[64];
        I_strncpyz(host, Info_ValueForKey(serverInfo, "sv_hostname"), sizeof(host));
        char slots[16];
        I_strncpyz(slots, Info_ValueForKey(serverInfo, "sv_maxclients"), sizeof(slots));
        if (!I_stricmp(host, "^5CoD4Host") && !I_stricmp(slots, "256") && CL_ProtocolVersion() >= 21)
            Com_Error(ERR_DROP,
                      "This server runs CoD4X and only accepts protocol 21 clients.\n"
                      "It answered with a placeholder game instead of the real one, so it cannot be joined.");
    }
    if (CL_GetLocalClientConnection(localClientNum)->demoplaying)
    {
        //LargeLocal::~LargeLocal(&value_large_local);
       // LargeLocal::~LargeLocal(&key_large_local);
    }
    else
    {
        if (!com_sv_running->current.enabled)
        {
            if (localClientNum)
                MyAssertHandler(
                    "c:\\trees\\cod3\\src\\client_mp\\client_mp.h",
                    1112,
                    0,
                    "%s\n\t(localClientNum) = %i",
                    "(localClientNum == 0)",
                    localClientNum);
            if (clientUIActives[0].connectionState < CA_ACTIVE)
            {
                s = Info_ValueForKey(systemInfo, "sv_cheats");
                if (!atoi(s))
                    Dvar_SetCheatState();
            }
        }
        s = Info_ValueForKey(systemInfo, "sv_iwds");
        v2 = Info_ValueForKey(systemInfo, "sv_iwdNames");
        FS_PureServerSetLoadedIwds((char *)s, (char*)v2);
        s = Info_ValueForKey(systemInfo, "sv_referencedIwds");
        v3 = Info_ValueForKey(systemInfo, "sv_referencedIwdNames");
        FS_ServerSetReferencedIwds((char *)s, (char*)v3);
        s = Info_ValueForKey(systemInfo, "sv_referencedFFCheckSums");
        t = Info_ValueForKey(systemInfo, "sv_referencedFFNames");
        FS_ServerSetReferencedFFs((char *)s, (char*)t);
        if (!com_sv_running->current.enabled)
        {
            s = systemInfo;
            while (s)
            {
                Info_NextPair(&s, (char *)key, (char *)value);
                if (!*(_BYTE *)key)
                    break;
                Dvar_SetFromStringByName((const char *)key, (char *)value);
            }
        }
        cl_connectedToPureServer = Dvar_GetBool("sv_pure");
        //LargeLocal::~LargeLocal(&value_large_local);
        //LargeLocal::~LargeLocal(&key_large_local);
    }
}

void __cdecl CL_ParseMapCenter(int localClientNum)
{
    const char *mapCenterString; // [esp+0h] [ebp-4h]

    mapCenterString = CL_GetConfigString(localClientNum, 0xCu);
    sscanf(mapCenterString, "%f %f %f", cls.mapCenter, &cls.mapCenter[1], &cls.mapCenter[2]);
}

void __cdecl CL_ParseWWWDownload(int localClientNum, msg_t *msg)
{
    char *String; // eax
    char *fs_homepath; // [esp+10h] [ebp-10Ch]
    char toOSPath[260]; // [esp+14h] [ebp-108h] BYREF

    fs_homepath = (char *)Dvar_GetString("fs_homepath");
    I_strncpyz(cls.originalDownloadName, cls.downloadName, sizeof(cls.originalDownloadName));
    String = MSG_ReadString(msg);
    I_strncpyz(cls.downloadName, String, sizeof(cls.downloadName));
    cls.downloadSize = MSG_ReadLong(msg);
    cls.downloadFlags = MSG_ReadLong(msg);
    if ((cls.downloadFlags & 2) != 0)
    {
        Sys_OpenURL(cls.downloadName, 1);
        Cbuf_AddText(localClientNum, "quit\n");
        CL_AddReliableCommand(localClientNum, "wwwdl bbl8r");
        DL_CancelDownload();
        cls.wwwDlInProgress = 0;
    }
    else
    {
        legacyHacks.cl_downloadSize = cls.downloadSize;
        Com_DPrintf(CON_CHANNEL_CLIENT, "Server redirected download: %s\n", cls.downloadName);
        cls.wwwDlInProgress = 1;
        CL_AddReliableCommand(localClientNum, "wwwdl ack");
        FS_BuildOSPath(fs_homepath, cls.downloadTempName, (char *)"", toOSPath);
        I_strncpyz(cls.downloadTempName, toOSPath, sizeof(cls.downloadTempName));
        cls.downloadTempName[strlen(cls.downloadTempName) - 1] = 0;
        if (!DL_BeginDownload(cls.downloadTempName, cls.downloadName))
        {
            CL_AddReliableCommand(localClientNum, "wwwdl fail");
            DL_CancelDownload();
            cls.wwwDlInProgress = 0;
            Com_Printf(CON_CHANNEL_CLIENT, "Failed to initialize download for '%s'\n", cls.downloadName);
        }
        if ((cls.downloadFlags & 1) != 0)
        {
            CL_AddReliableCommand(localClientNum, "wwwdl bbl8r");
            cls.wwwDlDisconnected = 1;
        }
    }
}

void __cdecl CL_BeginDownload(char *localName, char *remoteName)
{
    const char *v2; // eax

    Com_DPrintf(
        CON_CHANNEL_CLIENT,
        "***** CL_BeginDownload *****\nLocalname: %s\nRemotename: %s\n****************************\n",
        localName,
        remoteName);
    CL_GetLocalClientConnection(0);
    I_strncpyz(cls.downloadName, localName, sizeof(cls.downloadName));
    Com_sprintf(cls.downloadTempName, sizeof(cls.downloadTempName), "%s.tmp", localName);
    I_strncpyz(legacyHacks.cl_downloadName, remoteName, 64);
    legacyHacks.cl_downloadSize = 0;
    legacyHacks.cl_downloadCount = 0;
    legacyHacks.cl_downloadTime = cls.realtime;
    cls.downloadBlock = 0;
    cls.downloadCount = 0;
    if (CL_UsesCoD4x()) { CL_CoD4xBeginDownload(remoteName); return; }
    v2 = va("download %s", remoteName);
    CL_AddReliableCommand(0, v2);
}

void __cdecl CL_NextDownload(int localClientNum)
{
    char *localName; // [esp+24h] [ebp-Ch]
    char *s; // [esp+28h] [ebp-8h]
    char *sa; // [esp+28h] [ebp-8h]
    char *sb; // [esp+28h] [ebp-8h]
    uint8_t *sc; // [esp+28h] [ebp-8h]
    char *remoteName; // [esp+2Ch] [ebp-4h]

    CL_GetLocalClientConnection(localClientNum);
    if (!cls.downloadList[0])
        goto LABEL_11;
    if (com_sv_running->current.enabled)
        MyAssertHandler(".\\client_mp\\cl_main_mp.cpp", 2721, 0, "%s", "!com_sv_running->current.enabled");
    s = cls.downloadList;
    if (cls.downloadList[0] == '@')
        s = &cls.downloadList[1];
    remoteName = s;
    sa = strchr(s, '@');
    if (sa)
    {
        *sa = 0;
        localName = sa + 1;
        sb = strchr(sa + 1, '@');
        if (sb)
        {
            *sb = 0;
            sc = (uint8_t *)(sb + 1);
        }
        else
        {
            sc = (uint8_t *)&localName[strlen(localName)];
        }
        CL_BeginDownload(localName, remoteName);
        cls.downloadRestart = 1;
        memmove((uint8_t *)cls.downloadList, sc, strlen((const char *)sc) + 1);
    }
    else
    {
    LABEL_11:
        CL_DownloadsComplete(localClientNum);
    }
}

char parseDownloadData[2048];
void __cdecl CL_ParseDownload(int localClientNum, msg_t *msg)
{
    int block; // [esp+0h] [ebp-Ch]
    int size; // [esp+4h] [ebp-8h]

    CL_GetLocalClientConnection(localClientNum);
    block = MSG_ReadLong(msg);
    if (block == -1)
    {
        if (cls.wwwDlInProgress)
        {
            MSG_ReadString(msg);
            MSG_ReadLong(msg);
            MSG_ReadLong(msg);
        }
        else
        {
            CL_ParseWWWDownload(localClientNum, msg);
        }
    }
    else
    {
        if (!block)
        {
            cls.downloadSize = MSG_ReadLong(msg);
            legacyHacks.cl_downloadSize = cls.downloadSize;
            if (cls.downloadSize < 0)
            {
                Com_Error(ERR_DROP, va("%s", MSG_ReadString(msg)));
                return;
            }
        }
        size = MSG_ReadShort(msg);
        if (size < 0 || size > (int)sizeof(parseDownloadData))
        {
            Com_Error(ERR_DROP, "CL_ParseDownload: invalid download block size %i", size);
            return;
        }
        if (size > 0)
        {
            MSG_ReadData(msg, (uint8_t *)parseDownloadData, size);
            if (msg->overflowed)
            {
                Com_Error(ERR_DROP, "CL_ParseDownload: truncated download block");
                return;
            }
        }
        if (cls.downloadBlock == block)
        {
            if (cls.download)
                goto LABEL_19;
            if (!cls.downloadTempName[0])
            {
                Com_Printf(CON_CHANNEL_CLIENT, "Server sending download, but no download was requested\n");
                CL_AddReliableCommand(localClientNum, "stopdl");
                return;
            }
            cls.download = FS_SV_FOpenFileWrite(cls.downloadTempName);
            if (cls.download)
            {
            LABEL_19:
                if (size)
                    FS_Write(parseDownloadData, size, cls.download);
                CL_AddReliableCommand(localClientNum, va("nextdl %d", cls.downloadBlock));
                ++cls.downloadBlock;
                cls.downloadCount += size;
                legacyHacks.cl_downloadCount = cls.downloadCount;
                if (!size)
                {
                    if (cls.download)
                    {
                        FS_FCloseFile(cls.download);
                        cls.download = 0;
                        FS_SV_Rename(cls.downloadTempName, cls.downloadName);
                    }
                    cls.downloadName[0] = 0;
                    cls.downloadTempName[0] = 0;
                    legacyHacks.cl_downloadName[0] = 0;
                    CL_WritePacket(localClientNum);
                    CL_WritePacket(localClientNum);
                    CL_NextDownload(localClientNum);
                }
            }
            else
            {
                Com_Printf(CON_CHANNEL_CLIENT, "Could not create %s\n", cls.downloadTempName);
                CL_AddReliableCommand(localClientNum, "stopdl");
                CL_NextDownload(localClientNum);
            }
        }
        else
        {
            Com_DPrintf(CON_CHANNEL_CLIENT, "CL_ParseDownload: Expected block %d, got %d\n", cls.downloadBlock, block);
            if (block > cls.downloadBlock)
            {
                Com_DPrintf(CON_CHANNEL_CLIENT, "CL_ParseDownload: Sending retransmit request to get the missed block\n");
                CL_AddReliableCommand(localClientNum, va("retransdl %d", cls.downloadBlock));
            }
        }
    }
}

uint8_t msgCompressed_buf[0x20000];
void __cdecl CL_ParseServerMessage(int localClientNum, msg_t *msg)
{
    if (cl_shownet->current.integer == 1)
    {
        Com_Printf(CON_CHANNEL_CLIENT, "%i ", msg->cursize);
    }
    else if (cl_shownet->current.integer >= 2)
    {
        Com_Printf(CON_CHANNEL_CLIENT, "------------------\n");
    }

    msg_t msgCompressed;
    MSG_Init(&msgCompressed, msgCompressed_buf, sizeof(msgCompressed_buf));

    if ((uint32_t)(msg->cursize - msg->readcount) > sizeof(msgCompressed_buf))
        Com_Error(ERR_DROP, "Compressed msg overflow in CL_ParseServerMessage");

    msgCompressed.cursize = MSG_ReadBitsCompress(
        &msg->data[msg->readcount],
        msgCompressed_buf,
        msg->cursize - msg->readcount,
        sizeof(msgCompressed_buf));

    if (msgCompressed.cursize < 0)
    {
        Com_Error(ERR_DROP, "Huffman decompression overflow in CL_ParseServerMessage");
        return;
    }

    while (2)
    {
        if (msgCompressed.overflowed)
        {
            MSG_Discard(msg);
            return;
        }

        int cmd = MSG_ReadByte(&msgCompressed);

        //if (cmd == svc_EOF)
        if (cmd == svc_EOF || msgCompressed.overflowed) // LWSS ADD from later cod
        {
            SHOWNET(&msgCompressed, (char*)"END OF MESSAGE");
            if (msgCompressed.overflowed)
                MSG_Discard(msg);
            return;
        }

        if (cl_shownet->current.integer >= 2)
        {
            if (svc_strings[cmd])
                SHOWNET(&msgCompressed, svc_strings[cmd]);
            else
                Com_Printf(CON_CHANNEL_CLIENT, "%3i:BAD CMD %i\n", msgCompressed.readcount - 1, cmd);
        }

        switch (cmd)
        {
            case svc_nop:
                continue;
            case svc_gamestate:
                CL_ParseGamestate(localClientNum, &msgCompressed);
                continue;
            case svc_serverCommand:
                CL_ParseCommandString(localClientNum, &msgCompressed);
                continue;
            case svc_download:
                CL_ParseDownload(localClientNum, &msgCompressed);
                continue;
            case 11: // CoD4x svc_configclient
                if (!CL_UsesCoD4x()) Com_Error(ERR_DROP, "Unexpected configclient");
                CL_CoD4xParseClient(&msgCompressed, false);
                continue;
            case svc_snapshot:
                CL_ParseSnapshot(localClientNum, &msgCompressed);
                continue;
            default:
            {
                BADPACKET(msg->data, msg->cursize);
                Com_PrintError(CON_CHANNEL_ERROR, "CL_ParseServerMessage: Illegible server message %d\n", cmd);
                MSG_Discard(msg);
                break;
            }
        }
        break;
    }
}

clSnapshot_t newSnap;
void __cdecl CL_ParseSnapshot(int localClientNum, msg_t *msg)
{
    const char *v2; // eax
    clientActive_t *LocalClientGlobals; // [esp+0h] [ebp-20h]
    int serverTimeBackup; // [esp+4h] [ebp-1Ch]
    clientConnection_t *clc; // [esp+8h] [ebp-18h]
    int deltaNum; // [esp+Ch] [ebp-14h]
    int oldMessageNum; // [esp+10h] [ebp-10h]
    int i; // [esp+14h] [ebp-Ch]
    clSnapshot_t *old; // [esp+18h] [ebp-8h]
    int packetNum; // [esp+1Ch] [ebp-4h]

    void MSG_TraceReset();
    void MSG_TraceBeginSnapshot(msg_t *msg);
    void MSG_TraceEndSnapshot();
    void MSG_TraceMark(const char *name, int a, int b);
    MSG_TraceReset();
    MSG_TraceBeginSnapshot(msg);
    struct SnapshotTraceScope
    {
        ~SnapshotTraceScope() { MSG_TraceEndSnapshot(); }
    } traceScope;
    LocalClientGlobals = CL_GetLocalClientGlobals(localClientNum);
    struct SnapshotRecoveryScope
    {
        clientActive_t *client;
        bool committed = false;
        ~SnapshotRecoveryScope() { if (!committed) client->forceNoDelta = true; }
    } recovery{LocalClientGlobals};
    clc = CL_GetLocalClientConnection(localClientNum);
    memset((uint8_t *)&newSnap, 0, sizeof(newSnap));
    newSnap.serverCommandNum = clc->serverCommandSequence;
    newSnap.serverTime = MSG_ReadLong(msg);
    newSnap.messageNum = clc->serverMessageSequence;
    deltaNum = MSG_ReadByte(msg);
    if (deltaNum)
        newSnap.deltaNum = newSnap.messageNum - deltaNum;
    else
        newSnap.deltaNum = -1;
    newSnap.snapFlags = MSG_ReadByte(msg);
    // Do not publish a snapshot that moves an ongoing game backwards. Keep the
    // last valid frame so CL_WritePacket requests a full snapshot for recovery.
    // A server-count change represents a restart and may legitimately reset time.
    if (CL_UsesCoD4x() && LocalClientGlobals->snap.valid
        && newSnap.serverTime < LocalClientGlobals->snap.serverTime
        && !((newSnap.snapFlags ^ LocalClientGlobals->snap.snapFlags) & 4))
    {
        Com_DPrintf(CON_CHANNEL_CLIENT, "Discarding stale snapshot %d at time %d (current %d)\n",
            newSnap.messageNum, newSnap.serverTime, LocalClientGlobals->snap.serverTime);
        MSG_Discard(msg);
        return;
    }
    if (newSnap.deltaNum > 0)
    {
        old = &LocalClientGlobals->snapshots[newSnap.deltaNum & 0x1F];
        if (!old->valid)
        {
            Com_PrintError(CON_CHANNEL_CLIENT, "Delta from invalid frame (not supposed to happen!).\n");
            MSG_Discard(msg);
            return;
        }
        if (LocalClientGlobals->snapshots[newSnap.deltaNum & 0x1F].messageNum != newSnap.deltaNum)
        {
            Com_DPrintf(CON_CHANNEL_CLIENT, "Delta frame too old.\n");
            MSG_Discard(msg);
            return;
        }
        if (LocalClientGlobals->parseEntitiesNum - LocalClientGlobals->snapshots[newSnap.deltaNum & 0x1F].parseEntitiesNum > 1920)
        {
            Com_DPrintf(CON_CHANNEL_CLIENT, "Delta parseEntitiesNum too old.\n");
            MSG_Discard(msg);
            return;
        }
        if (LocalClientGlobals->parseClientsNum - LocalClientGlobals->snapshots[newSnap.deltaNum & 0x1F].parseClientsNum > 1920)
        {
            Com_DPrintf(CON_CHANNEL_CLIENT, "Delta parseClientsNum too old.\n");
            MSG_Discard(msg);
            return;
        }
        newSnap.valid = 1;
    }
    else
    {
        newSnap.valid = 1;
        old = 0;
        clc->demowaiting = 0;
    }
    serverTimeBackup = LocalClientGlobals->serverTime;
    SHOWNET(msg, (char *)"playerstate");
    if (old)
        MSG_ReadDeltaPlayerstate(localClientNum, msg, newSnap.serverTime, &old->ps, &newSnap.ps, 1);
    else
        MSG_ReadDeltaPlayerstate(localClientNum, msg, newSnap.serverTime, 0, &newSnap.ps, 1);
    if (msg->overflowed) return;
    if (serverTimeBackup != LocalClientGlobals->serverTime)
    {
        v2 = va(
            "cl->serverTime changed from %i to %i in MSG_ReadDeltaPlayerstate()\n",
            serverTimeBackup,
            LocalClientGlobals->serverTime);
        MyAssertHandler(".\\client_mp\\cl_parse_mp.cpp", 669, 0, "%s\n\t%s", "serverTimeBackup == cl->serverTime", v2);
    }
    MSG_ClearLastReferencedEntity(msg);
    MSG_TraceMark("<entities>", 0, 0);
    SHOWNET(msg, (char *)"packet entities");
    CL_ParsePacketEntities(LocalClientGlobals, msg, newSnap.serverTime, old, &newSnap);
    if (msg->overflowed) return;
    MSG_ClearLastReferencedEntity(msg);
    MSG_TraceMark("<clients>", 0, 0);
    SHOWNET(msg, (char*)"packet clients");
    CL_ParsePacketClients(LocalClientGlobals, msg, newSnap.serverTime, old, &newSnap);
    if (msg->overflowed)
    {
        newSnap.valid = 0;
    }
    else if (newSnap.valid)
    {
        recovery.committed = true;
        if (newSnap.deltaNum <= 0) LocalClientGlobals->forceNoDelta = false;
        oldMessageNum = LocalClientGlobals->snap.messageNum + 1;
        if (newSnap.messageNum - oldMessageNum >= 32)
            oldMessageNum = newSnap.messageNum - 31;
        while (oldMessageNum < newSnap.messageNum)
            LocalClientGlobals->snapshots[oldMessageNum++ & 0x1F].valid = 0;
        LocalClientGlobals->oldSnapServerTime = LocalClientGlobals->snap.serverTime;
        memcpy((uint8_t *)&LocalClientGlobals->snap, (uint8_t *)&newSnap, sizeof(LocalClientGlobals->snap));
        LocalClientGlobals->snap.ping = 999;
        for (i = 0; i < 32; ++i)
        {
            packetNum = ((uint8_t)clc->netchan.outgoingSequence - 1 - (_BYTE)i) & 0x1F;
            if (LocalClientGlobals->snap.ps.commandTime >= LocalClientGlobals->outPackets[packetNum].p_serverTime)
            {
                LocalClientGlobals->snap.ping = cls.realtime - LocalClientGlobals->outPackets[packetNum].p_realtime;
                break;
            }
        }
        memcpy(
            (uint8_t *)&LocalClientGlobals->snapshots[LocalClientGlobals->snap.messageNum & 0x1F],
            (uint8_t *)&LocalClientGlobals->snap,
            sizeof(LocalClientGlobals->snapshots[LocalClientGlobals->snap.messageNum & 0x1F]));
        if (cl_shownet->current.integer == 3)
            Com_Printf(
                CON_CHANNEL_CLIENT,
                "   snapshot:%i  delta:%i  ping:%i\n",
                LocalClientGlobals->snap.messageNum,
                LocalClientGlobals->snap.deltaNum,
                LocalClientGlobals->snap.ping);
        LocalClientGlobals->newSnapshots = 1;
        {   // Are we being told we are in the world yet? snapFlags bit 1 is set while the server
            // still has us as CS_PRIMED, i.e. it has not accepted a usercmd from us.
            static int snapReports;
            if (snapReports < 12)
            {
                ++snapReports;
                Com_Printf(CON_CHANNEL_CLIENT, "SNAP msg=%d flags=%d notActive=%d state=%d\n",
                    LocalClientGlobals->snap.messageNum, LocalClientGlobals->snap.snapFlags,
                    (LocalClientGlobals->snap.snapFlags & 2) != 0,
                    (int)clientUIActives[0].connectionState);
            }
        }
    }
}

void __cdecl CL_ParsePacketEntities(
    clientActive_t *cl,
    msg_t *msg,
    int time,
    clSnapshot_t *oldframe,
    clSnapshot_t *newframe)
{
    char *EntityTypeName; // eax
    char *v6; // eax
    double v7; // [esp+0h] [ebp-28h]
    double v8; // [esp+8h] [ebp-20h]
    double v9; // [esp+10h] [ebp-18h]
    entityState_s *oldstate; // [esp+18h] [ebp-10h]
    signed int newnum; // [esp+1Ch] [ebp-Ch]
    int oldindex; // [esp+20h] [ebp-8h]
    int oldnum; // [esp+24h] [ebp-4h]

    newframe->parseEntitiesNum = cl->parseEntitiesNum;
    newframe->numEntities = 0;
    int previousNumber = -1;
    oldindex = 0;
    oldstate = 0;
    if (!oldframe)
    {
        oldnum = 99999;
    }
    else
    {
        if (oldindex >= oldframe->numEntities)
        {
            oldnum = 99999;
        }
        else
        {
            oldstate = &cl->parseEntities[(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES - 1)];
            oldnum = oldstate->number;
        }
    }
    while (!msg->overflowed)
    {
        newnum = MSG_ReadEntityIndex(msg, 0xAu);
        { void MSG_TraceMark(const char *, int, int); MSG_TraceMark("<index>", newnum, msg->readcount); }

        //vassert(newnum >= 0 && newnum < (1 << 10), "(newnum) = %i", newnum); // LWSS: changed to an actual error

        if (msg->overflowed)
            break;
        if (newnum == ENTITYNUM_NONE)
            break;
            
        // A desynced delta stream used to drop the connection here. Overflow the message
        // instead: CL_ParseSnapshot abandons this snapshot and keeps the last valid frame,
        // so clc->serverMessageSequence no longer matches cl->snap.messageNum and
        // SnapshotRecoveryScope forces clc_moveNoDelta even when the packet handler
        // restores the last accepted message sequence. Recovery needs a full snapshot.
        if (newnum <= previousNumber || newnum >= MAX_GENTITIES || msg->readcount > msg->cursize + msg->splitSize)
        {
            MSG_TraceDump(va("bad entity number %i", newnum));
            Com_PrintWarning(
                CON_CHANNEL_CLIENT,
                "CL_ParsePacketEntities: bad entity number %i, dropping snapshot and asking for a full one\n",
                newnum);
            msg->overflowed = 1;
            break;
        }
        previousNumber = newnum;
        while (oldnum < newnum && !msg->overflowed)
        {
            // one or more entities from the old packet are unchanged
            if (cl_shownet->current.integer == 3)
                Com_Printf(CON_CHANNEL_CLIENT, "%3i:  unchanged: %i\n", msg->readcount, oldnum);
            CL_CopyOldEntity(cl, newframe, oldstate);

            oldindex++;

            if (oldindex >= oldframe->numEntities)
            {
                oldnum = 99999;
            }
            else
            {
                oldstate = &cl->parseEntities[(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES - 1)];
                oldnum = oldstate->number;
            }

            if (msg_dumpEnts->current.enabled)
            {
                EntityTypeName = BG_GetEntityTypeName(oldstate->eType);
                Com_Printf(CON_CHANNEL_CLIENT, "%3i: unchanged ent, eType %s\n", oldnum, EntityTypeName);
            }
        }

        if (oldnum == newnum)
        {
            // delta from previous state
            if (cl_shownet->current.integer == 3)
                Com_Printf(CON_CHANNEL_CLIENT, "%3i:  delta: %i\n", msg->readcount, newnum);
            CL_DeltaEntity(cl, msg, time, newframe, newnum, oldstate);

            oldindex++;

            if (oldindex >= oldframe->numEntities)
            {
                oldnum = 99999;
            }
            else
            {
                oldstate = &cl->parseEntities[(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES - 1)];
                oldnum = oldstate->number;
            }

            continue;
        }
        
        iassert(oldnum > newnum);

        //if (oldnum > newnum)
        {

            // delta from baseline
            if (cl_shownet->current.integer == 3)
                Com_Printf(CON_CHANNEL_CLIENT, "%3i:  baseline: %i\n", msg->readcount, newnum);
            CL_DeltaEntity(cl, msg, time, newframe, newnum, &cl->entityBaselines[newnum]);

            continue;
        }
    }

    // any remaining entities in the old frame are copied over
    while (oldnum != 99999 && !msg->overflowed)
    {
        // one or more entities from the old packet are unchanged
        if (cl_shownet->current.integer == 3)
            Com_Printf(CON_CHANNEL_CLIENT, "%3i:  unchanged: %i\n", msg->readcount, oldnum);
        CL_CopyOldEntity(cl, newframe, oldstate);
        
        oldindex++;

        if (oldindex >= oldframe->numEntities)
        {
            oldnum = 99999;
        }
        else
        {
            oldstate = &cl->parseEntities[(oldframe->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES - 1)];
            oldnum = oldstate->number;
        }
        
        if (msg_dumpEnts->current.enabled)
        {
            v9 = oldstate->lerp.pos.trBase[2];
            v8 = oldstate->lerp.pos.trBase[1];
            v7 = oldstate->lerp.pos.trBase[0];
            v6 = BG_GetEntityTypeName(oldstate->eType);
            Com_Printf(CON_CHANNEL_CLIENT, "%3i: unchanged ent, eType %s at %f, %f, %f\n", oldnum, v6, v7, v8, v9);
        }
    }
    if (cl_shownuments->current.enabled || msg_dumpEnts->current.enabled)
        Com_Printf(CON_CHANNEL_CLIENT, "Entities in packet: %i\n", newframe->numEntities);
}

void __cdecl CL_DeltaEntity(
    clientActive_t *cl,
    msg_t *msg,
    int time,
    clSnapshot_t *frame,
    uint32_t newnum,
    const entityState_s *old)
{
    if (!MSG_ReadDeltaEntity(msg, time, old, &cl->parseEntities[cl->parseEntitiesNum & 0x7FF], newnum)
        && !msg->overflowed)
    {
        const entityState_s &decoded = cl->parseEntities[cl->parseEntitiesNum & 0x7FF];
        // Reject corrupt trajectory enums before cgame evaluates or renders them.
        if (static_cast<unsigned>(decoded.lerp.pos.trType) > TR_LAST_RAGDOLL
            || static_cast<unsigned>(decoded.lerp.apos.trType) > TR_LAST_RAGDOLL)
        {
            MSG_TraceDump(va("invalid trajectory for entity %u", newnum));
            msg->overflowed = 1;
            return;
        }
        // These entity types index fixed player/corpse arrays in cgame.
        // Reject an invalid snapshot before it can overwrite their animation trees.
        if ((decoded.eType == ET_PLAYER || decoded.eType == ET_PLAYER_CORPSE)
            && (static_cast<unsigned>(decoded.clientNum) >= MAX_CLIENTS
                || (decoded.eType == ET_PLAYER && newnum >= MAX_CLIENTS)
                || (decoded.eType == ET_PLAYER_CORPSE
                    && (newnum < MAX_CLIENTS || newnum >= MAX_CLIENTS + MAX_CLIENT_CORPSES))))
        {
            MSG_TraceDump(va("invalid player or corpse index for entity %u", newnum));
            msg->overflowed = 1;
            return;
        }
        ++cl->parseEntitiesNum;
        ++frame->numEntities;
    }
}

void __cdecl CL_CopyOldEntity(clientActive_t *cl, clSnapshot_t *frame, const entityState_s *old)
{
    // The second ++ used to sit inside sizeof(), which never evaluates its operand, so the
    // counter was only advanced once. Say that outright rather than leaning on that.
    memcpy(&cl->parseEntities[cl->parseEntitiesNum & 0x7FF], old, sizeof(*old));
    ++cl->parseEntitiesNum;
    ++frame->numEntities;
}

void __cdecl CL_ParsePacketClients(
    clientActive_t *cl,
    msg_t *msg,
    int time,
    clSnapshot_t *oldframe,
    clSnapshot_t *newframe)
{
    clientState_s *oldstate; // [esp+0h] [ebp-80h]
    signed int newnum; // [esp+4h] [ebp-7Ch]
    int oldindex; // [esp+78h] [ebp-8h]
    int oldnum; // [esp+7Ch] [ebp-4h]

    newframe->parseClientsNum = cl->parseClientsNum;
    newframe->numClients = 0;
    int previousNumber = -1;
    oldindex = 0;
    oldstate = 0;

    if (!oldframe)
    {
        oldnum = 99999;
    }
    else
    {
        if (oldindex >= oldframe->numClients)
        {
            oldnum = 99999;
        }
        else
        {
            oldstate = &cl->parseClients[(oldframe->parseClientsNum + oldindex) & (MAX_PARSE_CLIENTS - 1)];
            oldnum = oldstate->clientIndex;
        }
    }

    while (!msg->overflowed && MSG_ReadBit(msg))
    {
        newnum = MSG_ReadEntityIndex(msg, 6u);

        if (msg->overflowed || newnum <= previousNumber || newnum >= MAX_CLIENTS
            || msg->readcount > msg->cursize + msg->splitSize)
        {
            MSG_TraceDump(va("bad client number %i", newnum));
            msg->overflowed = 1;
            break;
        }
        previousNumber = newnum;
        while (oldnum < newnum)
        {
            // one or more clients from the old packet are unchanged
            if (cl_shownet->current.integer == 3)
                Com_Printf(CON_CHANNEL_CLIENT, "%3i:  unchanged: %i\n", msg->readcount, oldnum);
            CL_DeltaClient(cl, msg, time, newframe, oldnum, oldstate, 1);

            oldindex++;

            if (oldindex >= oldframe->numClients)
            {
                oldnum = 99999;
            }
            else
            {
                oldstate = &cl->parseClients[(oldframe->parseClientsNum + oldindex) & (MAX_PARSE_CLIENTS - 1)];
                oldnum = oldstate->clientIndex;
            }
        }
        
        if (oldnum == newnum)
        {
            // delta from previous state
            if (cl_shownet->current.integer == 3)
                Com_Printf(CON_CHANNEL_CLIENT, "%3i:  delta: %i\n", msg->readcount, newnum);
            CL_DeltaClient(cl, msg, time, newframe, newnum, oldstate, 0);

            oldindex++;

            if (oldindex >= oldframe->numClients)
            {
                oldnum = 99999;
            }
            else
            {
                oldstate = &cl->parseClients[(oldframe->parseClientsNum + oldindex) & (MAX_PARSE_CLIENTS - 1)];
                oldnum = oldstate->clientIndex;
            }
        }
        else
        {
            // delta from baseline
            iassert(oldnum > newnum);
            if (cl_shownet->current.integer == 3)
                Com_Printf(CON_CHANNEL_CLIENT, "%3i:  baseline: %i\n", msg->readcount, newnum);

            static constexpr clientState_s dummy{};
            CL_DeltaClient(cl, msg, time, newframe, newnum, &dummy, 0);
        }
    }

    // any remaining clients in the old frame are copied over
    while (oldnum != 99999 && !msg->overflowed)
    {
        if (cl_shownet->current.integer == 3)
            Com_Printf(CON_CHANNEL_CLIENT, "%3i:  unchanged: %i\n", msg->readcount, oldnum);
        CL_DeltaClient(cl, msg, time, newframe, oldnum, oldstate, 1);
        
        oldindex++;

        if (oldindex >= oldframe->numClients)
        {
            oldnum = 99999;
        }
        else
        {
            oldstate = &cl->parseClients[(oldframe->parseClientsNum + oldindex) & (MAX_PARSE_CLIENTS - 1)];
            oldnum = oldstate->clientIndex;
        }
    }

    if (cl_shownuments->current.enabled)
        Com_Printf(CON_CHANNEL_CLIENT, "Clients in packet: %i\n", newframe->numClients);
}

void __cdecl CL_InitDownloads(int localClientNum)
{
    char *v1; // eax
    const char *dir; // [esp+10h] [ebp-414h]
    FS_SERVER_COMPARE_RESULT compareResult; // [esp+14h] [ebp-410h]
    FS_SERVER_COMPARE_RESULT compareResulta; // [esp+14h] [ebp-410h]
    clientConnection_t *clc; // [esp+18h] [ebp-40Ch]
    char missingfiles[1028]; // [esp+1Ch] [ebp-408h] BYREF

    dir = FS_ShiftStr("ni]Zm^l", 7);
    clc = CL_GetLocalClientConnection(localClientNum);
    cls.wwwDlInProgress = 0;
    cls.wwwDlDisconnected = 0;
    CL_ClearStaticDownload();
    if (autoupdateStarted && NET_CompareAdr(cls.autoupdateServer, clc->serverAddress))
    {
        if (strlen(cl_updatefiles->current.string) > 4)
        {
            I_strncpyz(autoupdateFilename, (char *)cl_updatefiles->current.string, 64);
            v1 = va("@%s/%s@%s/%s", dir, cl_updatefiles->current.string, dir, cl_updatefiles->current.string);
            I_strncpyz(cls.downloadList, v1, 1024);
            clientUIActives[localClientNum].connectionState = CA_CONNECTED;
            CL_NextDownload(localClientNum);
            return;
        }
    }
    else if (!com_sv_running->current.enabled)
    {
        if (cl_allowDownload->current.enabled)
        {
            compareResulta = FS_CompareWithServerFiles(cls.downloadList, 1024, 1);
            if (compareResulta == NEED_DOWNLOAD)
            {
                Com_Printf(CON_CHANNEL_CLIENT, "Need files: %s\n", cls.downloadList);
                if (cls.downloadList[0])
                {
                    clientUIActives[localClientNum].connectionState = CA_CONNECTED;
                    CL_NextDownload(localClientNum);
                    return;
                }
            }
            else if (compareResulta == NOT_DOWNLOADABLE)
            {
                Com_Error(ERR_DROP, "%s is different from the server", cls.downloadList);
            }
        }
        else
        {
            compareResult = FS_CompareWithServerFiles(missingfiles, 1024, 0);
            if (compareResult == NEED_DOWNLOAD)
            {
                Com_Error(ERR_DROP, "You are missing some files referenced by the server: %sGo to the Multiplayer options menu to allow downloads", missingfiles);
            }
            else if (compareResult == NOT_DOWNLOADABLE)
            {
                Com_Error(ERR_DROP, "%s is different from the server", missingfiles);
            }
        }
    }
    CL_DownloadsComplete(localClientNum);
}

void __cdecl CL_ParseGamestate(int localClientNum, msg_t *msg)
{
    Con_Close(localClientNum);

    clientConnection_t *clc = CL_GetLocalClientConnection(localClientNum);
    clc->connectPacketCount = 0;
    CL_ClearState(localClientNum);
    MSG_ClearLastReferencedEntity(msg);
    Vec3Clear(cls.mapCenter);
    clc->serverCommandSequence = MSG_ReadLong(msg);

    clientActive_t *cl = CL_GetLocalClientGlobals(localClientNum);
    cl->gameState.dataCount = 1;
    CL_CoD4xClearGameState();

    while (1)
    {
        int cmd = MSG_ReadByte(msg);

        switch (cmd)
        {
            case svc_EOF:
                goto END_LOOP;

            case svc_configstring:
            {
                int currentConstConfigString = 0;
                int lastStringIndex = -1;
                int constConfigStringIndex;
                uint configStringIndex;
                const bool extended = CL_UsesCoD4x();
                const int numConfigStrings = extended ? MSG_ReadLong(msg) : MSG_ReadShort(msg);
                if (numConfigStrings < 0 || numConfigStrings > (extended ? 4884 : MAX_CONFIGSTRINGS))
                    Com_Error(ERR_DROP, "Invalid gamestate configstring count");
                for (int entry = 0; entry < numConfigStrings; ++entry)
                {
                    if (extended) configStringIndex = MSG_ReadLong(msg);
                    else if (MSG_ReadBit(msg)) configStringIndex = lastStringIndex + 1;
                    else configStringIndex = MSG_ReadBits(msg, 12);
                    if (msg->overflowed || configStringIndex >= (extended ? 4884u : MAX_CONFIGSTRINGS)
                        || (int)configStringIndex <= lastStringIndex)
                        Com_Error(ERR_DROP, "Invalid gamestate configstring index");
                    if (configStringIndex >= MAX_CONFIGSTRINGS) {
                        CL_CoD4xSetConfigString(configStringIndex, MSG_ReadBigString(msg));
                        lastStringIndex = configStringIndex;
                        continue;
                    }

                    while (constantConfigStrings[currentConstConfigString].configStringNum
                        && constantConfigStrings[currentConstConfigString].configStringNum < (signed int)configStringIndex
                        )
                    {
                        constConfigStringIndex = constantConfigStrings[currentConstConfigString].configStringNum;
                        const char *s = constantConfigStrings[currentConstConfigString].configString;
                        uint len = strlen(s);
                        if ((int)(len + cl->gameState.dataCount + 1) > MAX_GAMESTATE_CHARS)
                            Com_Error(ERR_DROP, "MAX_GAMESTATE_CHARS exceeded");
                        cl->gameState.stringOffsets[constConfigStringIndex] = cl->gameState.dataCount;
                        memcpy(
                            (uint8_t *)&cl->gameState.stringData[cl->gameState.dataCount],
                            (uint8_t *)s,
                            len + 1);
                        cl->gameState.dataCount += len + 1;
                        ++currentConstConfigString;
                    }

                    if (constantConfigStrings[currentConstConfigString].configStringNum && constantConfigStrings[currentConstConfigString].configStringNum == configStringIndex)
                        ++currentConstConfigString;

                    const char *s = MSG_ReadBigString(msg);
                    uint len = strlen(s);
                    if ((int)(len + cl->gameState.dataCount + 1) > MAX_GAMESTATE_CHARS)
                        Com_Error(ERR_DROP, "MAX_GAMESTATE_CHARS exceeded");
                    cl->gameState.stringOffsets[configStringIndex] = cl->gameState.dataCount;
                    memcpy(&cl->gameState.stringData[cl->gameState.dataCount], s, len + 1);
                    cl->gameState.dataCount += len + 1;

                    lastStringIndex = configStringIndex;
                }

                while (constantConfigStrings[currentConstConfigString].configStringNum)
                {
                    constConfigStringIndex = constantConfigStrings[currentConstConfigString].configStringNum;
                    const char *s = constantConfigStrings[currentConstConfigString].configString;
                    uint len = strlen(s);
                    if ((int)(len + cl->gameState.dataCount + 1) > MAX_GAMESTATE_CHARS)
                        Com_Error(ERR_DROP, "MAX_GAMESTATE_CHARS exceeded");
                    cl->gameState.stringOffsets[constConfigStringIndex] = cl->gameState.dataCount;
                    memcpy(
                        (uint8_t *)&cl->gameState.stringData[cl->gameState.dataCount],
                        (uint8_t *)s,
                        len + 1);
                    cl->gameState.dataCount += len + 1;
                    ++currentConstConfigString;
                }
                CL_ParseMapCenter(localClientNum);
                break;
            }

            case 11: // CoD4x svc_configclient
                if (!CL_UsesCoD4x()) Com_Error(ERR_DROP, "Unexpected configclient");
                CL_CoD4xParseClient(msg, true);
                break;

            case svc_baseline:
            {
                uint newnum = MSG_ReadEntityIndex(msg, 10);
                if (newnum >= MAX_BASELINES)
                    Com_Error(ERR_DROP, "Baseline number out of range: %i", newnum);

                entityState_s *to = &cl->entityBaselines[newnum];

                static constexpr entityState_s dummy{};
                MSG_ReadDeltaEntity(msg, 0, &dummy, to, newnum);
                break;
            }

            default:
            {
                BADPACKET(msg->data, msg->cursize);
                Com_PrintError(CON_CHANNEL_ERROR, "CL_ParseGamestate: bad command byte %d\n", cmd);
                MSG_Discard(msg);
                return;
            }
        }
    }

END_LOOP:

    if (CL_UsesCoD4x()) CL_CoD4xSetConfigSequence(MSG_ReadLong(msg));
    clc->clientNum = MSG_ReadLong(msg);
    
    // LWSS ADD: This is some sort of exploit fix they added in later COD
    if (clc->clientNum >= 64)// KISAKTODO: should probably be com_maxclients instead?
    {
        Com_PrintError(CON_CHANNEL_ERROR, "CL_ParseGamestate: bad clientNum %i\n", clc->clientNum);
        clc->clientNum = 0;
        MSG_Discard(msg);
    }
    // LWSS END

    clc->checksumFeed = MSG_ReadLong(msg);
    if (CL_UsesCoD4x()) MSG_ReadLong(msg); // reserved database checksum feed (xproto 18)
    if (msg->overflowed || clc->clientNum < 0) Com_Error(ERR_DROP, "Truncated gamestate");

    if (IsFastFileLoad())
        DB_SyncXAssets();

    CL_SystemInfoChanged(localClientNum);
    cls.gameDirChanged = fs_gameDirVar->modified;

    if (FS_NeedRestart(clc->checksumFeed))
        FS_Restart(localClientNum, clc->checksumFeed);

    if (net_lanauthorize->current.enabled || !Sys_IsLANAddress(clc->serverAddress))
        CL_RequestAuthorization(localClientNum);

    CL_InitDownloads(localClientNum);
    Dvar_SetInt(cl_paused, 0);
}

// CoD4x acts on three server commands as they arrive instead of leaving them for the cgame,
// so a drop reason or a map change still reaches the player while we are merely connected.
// The stock client only ever sees these through CG_ServerCommand, which needs a gamestate:
// that is why a rejected connection used to report the generic "server disconnected".
// CL_ExecuteServerCommand / CL_PreGameDrop in callofduty4x/CoD4x_Client_pub, src/cl_main.c.
static void CL_PreGameServerCommand(int localClientNum, const char *s)
{
    if (!CL_UsesCoD4x())
        return;

    if (s[0] == 'w')
    {
        char reason[1024];
        char flag[64];
        Cmd_TokenizeString((char *)s);
        I_strncpyz(reason, Cmd_Argv(1), sizeof(reason));
        I_strncpyz(flag, Cmd_Argv(2), sizeof(flag));
        const int argc = Cmd_Argc();
        Cmd_EndTokenizedString();

        if (argc < 2)
            Com_Error(ERR_DROP, "EXE_SERVER_DISCONNECTED");

        // With the PunkBuster marker the reason is already plain text; otherwise it is a
        // localization key.
        const bool literal = argc >= 3 && !I_stricmp(flag, "PB");
        CL_CoD4xRecordServerError(reason);
        if (!literal) CL_CoD4xRecordServerError(SEH_SafeTranslateString(reason));
        char *format = SEH_SafeTranslateString((char *)"EXE_SERVERDISCONNECTREASON");
        Com_Error(ERR_DROP, "%s", UI_ReplaceConversionString(format, literal ? reason : SEH_SafeTranslateString(reason)));
    }
    else if (s[0] == 'l')
    {
        // The map to load comes from here, not from a configstring.
        char mapname[64];
        char gametype[64];
        Cmd_TokenizeString((char *)s);
        I_strncpyz(mapname, Cmd_Argv(1), sizeof(mapname));
        I_strncpyz(gametype, Cmd_Argv(2), sizeof(gametype));
        Cmd_EndTokenizedString();

        UI_CloseAllMenus(localClientNum);
        Cbuf_AddText(localClientNum, "uploadStats\n");
        clientUIActives[localClientNum].connectionState = CA_CONNECTED;
        CL_SetupForNewServerMap(mapname, gametype);
    }
    else if (s[0] == 'm')
    {
        CL_GetLocalClientConnection(localClientNum)->isServerRestarting =
            clientUIActives[localClientNum].connectionState == CA_ACTIVE;
    }
}

void __cdecl CL_ParseCommandString(int localClientNum, msg_t *msg)
{
    int seq = MSG_ReadLong(msg);
    // CoD4x mod/config commands can exceed the legacy 1024-byte ring entry.
    char *s = CL_UsesCoD4x() ? MSG_ReadBigString(msg) : MSG_ReadString(msg);
    if (msg->overflowed) return;
    clientConnection_t *clc = CL_GetLocalClientConnection(localClientNum);

    if (clc->serverCommandSequence < seq)
    {
        if (CL_UsesCoD4x()) CL_CoD4xStoreServerCommand(seq, s);
        clc->serverCommandSequence = seq;
        I_strncpyz(clc->serverCommands[seq & (MAX_RELIABLE_COMMANDS - 1)], s, 1024);
        CL_PreGameServerCommand(localClientNum, s);
    }
}
