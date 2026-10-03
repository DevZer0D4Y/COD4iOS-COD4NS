#include <universal/q_shared.h>
#include "client_mp.h"
#include "cl_cod4x.h"
#include <cgame_mp/cg_local_mp.h>
void CL_ParseWWWDownload(int localClientNum, msg_t *msg);
void CL_NextDownload(int localClientNum);
void CL_InitServerInfo(serverInfo_t *server, netadr_t adr);
#include <qcommon/qcommon.h>
#include <universal/com_files.h>
#include <win32/win_storage.h>
#include <zlib/zlib.h>
#include <array>
#include <string>
#include <vector>
#include <cstdio>
#include <ctime>
#include <stringed/stringed_hooks.h>
#include "../../ports/ios/network/cod4x_transport.h"
#include "../../ports/ios/network/cod4x_commands.h"
#include "../../ports/ios/network/cod4x_server_filter.h"
#include "../../ports/ios/network/cod4x_verified_servers.h"
#ifdef __APPLE__
#include <CommonCrypto/CommonCryptor.h>
#include "../../ports/ios/platform/apple_master.h"
#endif

namespace {
cod4x::ReliableTransport channel;
cod4x::CommandHistory<> commandHistory;
bool started;
bool steamReported;
int configSequence;
std::array<std::string, 2442> extendedStrings;
size_t extendedBytes;
struct ClientData { char name[33]; char clan[13]; } remoteClients[64];
bool clientValid[64];
struct Download { char name[64]; int size; uint32_t checksum; bool valid; } download;

// Local compatibility outcomes supplement the public verified-server baseline.
// The legacy cache's "steam" field now denotes explicit incompatible authentication.
using Verdict = cod4x::ServerVerdict;
std::vector<Verdict> verdicts;
bool verdictsLoaded;
const dvar_t *cl_hideSteamServers;

void verdictPath(char *out, size_t size) {
    char osPath[1024];
    FS_BuildOSPath(Dvar_GetString("fs_homepath"), (char *)"cod4x_servers.txt", (char *)"", osPath);
    size_t n = strlen(osPath);
    if (n && osPath[n - 1] == '/') osPath[n - 1] = 0;
    I_strncpyz(out, osPath, int(size));
}
void loadVerdicts() {
    if (verdictsLoaded) return;
    verdictsLoaded = true;
    char path[1024]; verdictPath(path, sizeof(path));
    FILE *file = fopen(path, "r");
    if (!file) return;
    char line[128];
    while (fgets(line, sizeof(line), file)) {
        unsigned a, b, c, d, port, steam; long long when;
        if (sscanf(line, "%u.%u.%u.%u:%u %u %lld", &a, &b, &c, &d, &port, &steam, &when) != 7) continue;
        if (a > 255 || b > 255 || c > 255 || d > 255 || !port || port > 65535 || steam > 1 || when <= 0) continue;
        verdicts.push_back({(a << 24) | (b << 16) | (c << 8) | d, uint16_t(port), steam != 0, when});
    }
    fclose(file);
}
void saveVerdicts() {
    char path[1024]; verdictPath(path, sizeof(path));
    FILE *file = fopen(path, "w");
    if (!file) return;
    for (const auto &v : verdicts)
        fprintf(file, "%u.%u.%u.%u:%u %d %lld\n", (v.ip >> 24) & 255, (v.ip >> 16) & 255,
                (v.ip >> 8) & 255, v.ip & 255, v.port, v.steam ? 1 : 0, (long long)v.when);
    fclose(file);
}
// Addresses are stored host-order; netadr_t keeps ip[] and port in network order.
void addressParts(const netadr_t &address, uint32_t &ip, uint16_t &port) {
    ip = (uint32_t(address.ip[0]) << 24) | (uint32_t(address.ip[1]) << 16)
       | (uint32_t(address.ip[2]) << 8) | uint32_t(address.ip[3]);
    port = uint16_t((address.port >> 8) | (address.port << 8));
}
void recordVerdict(bool steam) {
    if (clientUIActives[0].connectionState < CA_CONNECTING) return;
    uint32_t ip; uint16_t port;
    addressParts(CL_GetLocalClientConnection(0)->serverAddress, ip, port);
    if (!ip) return;
    loadVerdicts();
    const int64_t now = (int64_t)time(nullptr);
    for (auto &v : verdicts)
        if (v.ip == ip && v.port == port) {
            if (v.steam == steam && now - v.when < 60 * 60) return;
            v.steam = steam; v.when = now; saveVerdicts(); return;
        }
    verdicts.push_back({ip, port, steam, now});
    saveVerdicts();
}
// Empty Documents must have the same filtering policy as an established installation.
// Seeds contain independently tested public addresses, never an exported player's cache.
bool worthShowing(const netadr_t &address) {
    loadVerdicts();
    uint32_t ip; uint16_t port;
    addressParts(address, ip, port);
    const bool baseline = cod4x::VerifiedServer(ip, port);
    const int64_t now = (int64_t)time(nullptr);
    for (const auto &v : verdicts)
        if (v.ip == ip && v.port == port)
            return cod4x::ShowServer(baseline, &v, now);
    return cod4x::ShowServer(baseline, nullptr, now);
}
void send(msg_t *msg) {
    if (msg->overflowed || !channel.send(msg->data, msg->cursize))
        Com_Error(ERR_DROP, "CoD4x reliable send overflow");
}
void downloadMessage(int command, const char *name = nullptr, int offset = -1, int size = 0) {
    uint8_t bytes[1024]; msg_t msg; MSG_Init(&msg, bytes, sizeof(bytes));
    MSG_WriteLong(&msg, 5); MSG_WriteByte(&msg, command);
    if (name) MSG_WriteString(&msg, name);
    if (offset >= 0) { MSG_WriteLong(&msg, offset); MSG_WriteLong(&msg, size); }
    send(&msg);
}
// svc_steamcommands. Wire format and the replies below mirror CL_ProcessSteamCommands /
// CL_ProcessSteamAuthorizeRequest in callofduty4x/CoD4x_Client_pub, src/cl_main.c.
// There is no Steam runtime on iOS, so we answer with the protocol's own "no Steam" reply -
// the same one the PC client sends when Steam is not installed. Whether that is enough is the
// server's decision to make; we never claim a Steam identity we do not have.
void processSteamCommands(msg_t *request) {
    MSG_ReadLong(request); MSG_ReadLong(request);      // server steam id (int64, unused here)
    const int command = MSG_ReadLong(request);
    // Short messages are normal here: reading past the end yields command -1, the patch-status
    // request, which the public client ignores. Only command 0 asks us for an auth ticket.
    if (request->overflowed) { request->overflowed = 0; return; }
    if (command != 0) return;
    char subcommand[1024];
    I_strncpyz(subcommand, MSG_ReadString(request), sizeof(subcommand));
    if (request->overflowed) { request->overflowed = 0; return; }
    char verb[32]; size_t n = 0;
    for (const char *c = subcommand; *c && *c != ' ' && *c != '\t' && n + 1 < sizeof(verb); ++c) verb[n++] = *c;
    verb[n] = 0;
    if (!I_stricmp(verb, "reset")) return;
    if (I_stricmp(verb, "renew") && I_stricmp(verb, "waiting")) return;
    uint8_t bytes[64]; msg_t msg; MSG_Init(&msg, bytes, sizeof(bytes));
    MSG_WriteLong(&msg, 8);        // clc_steamcommands
    MSG_WriteByte(&msg, 1);        // subcommand: no Steam client
    MSG_WriteByte(&msg, 0);        // ...and none installed either
    send(&msg);
    if (!steamReported) {
        steamReported = true;
        // A request alone does not prove Steam is mandatory; wait for an explicit rejection.
        Com_Printf(CON_CHANNEL_CLIENT, "CoD4x: server asked for Steam authentication; reported that Steam is unavailable on this platform\n");
    }
}
void sendStats(msg_t *request) {
    if (MSG_ReadByte(request) != 0 || request->overflowed) return;
    uint8_t bytes[8220]; msg_t msg; MSG_Init(&msg, bytes, sizeof(bytes));
    MSG_WriteLong(&msg, 9);
    if (!LiveStorage_DoWeHaveStats()) MSG_WriteByte(&msg, 0);
    else {
#ifdef __APPLE__
        uint8_t encrypted[8192]; uint8_t key[16];
        const uint32_t challenge = CL_GetLocalClientConnection(0)->challenge;
        for (int i = 0; i < 16; ++i) key[i] = uint8_t(challenge >> (8 * (i % 4)));
        const uint8_t iv[16] = {0x4f,0x11,0x62,0xeb,0x44,0x61,0x99,0x66,0xa4,0xcf,0x41,0x73,0x99,0x12,0x55,0xb9};
        size_t written = 0;
        if (CCCrypt(kCCEncrypt, kCCAlgorithmAES, 0, key, sizeof(key), iv,
            LiveStorage_GetStatBuffer()->playerStats, 8192, encrypted, sizeof(encrypted), &written) != kCCSuccess || written != 8192)
            Com_Error(ERR_DROP, "CoD4x statistics encryption failed");
        MSG_WriteByte(&msg, 2); MSG_WriteLong(&msg, 8192); MSG_WriteData(&msg, encrypted, 8192);
#else
        MSG_WriteByte(&msg, 0);
#endif
    }
    send(&msg);
}
void parseDownload(msg_t *msg) {
    const int command = MSG_ReadByte(msg);
    if (command == 3) Com_Error(ERR_DROP, "%s", MSG_ReadString(msg));
    if (!download.name[0]) Com_Error(ERR_DROP, "Unrequested CoD4x download");
    if (command == 1) {
        const int size = MSG_ReadLong(msg);
        MSG_ReadLong(msg); // checksum structure size
        if (msg->cursize - msg->readcount != 64 + 4 + 256 * 4 + 4)
            Com_Error(ERR_DROP, "Invalid CoD4x download checksum block");
        char name[64]; MSG_ReadData(msg, (uint8_t *)name, sizeof(name));
        const int confirmedSize = MSG_ReadLong(msg);
        for (int i = 0; i < 256; ++i) MSG_ReadLong(msg); // optional cached-segment CRCs
        download.checksum = uint32_t(MSG_ReadLong(msg));
        if (!memchr(name, 0, sizeof(name)) || strcmp(name, download.name) || size <= 0 || size != confirmedSize || msg->overflowed)
            Com_Error(ERR_DROP, "Invalid CoD4x download metadata");
        download.size = size; download.valid = true;
        cls.downloadSize = size; cls.downloadCount = 0;
        legacyHacks.cl_downloadSize = size;
        if (cls.download) FS_FCloseFile(cls.download);
        cls.download = FS_SV_FOpenFileWrite(cls.downloadTempName);
        if (!cls.download) Com_Error(ERR_DROP, "Could not create %s", cls.downloadTempName);
        downloadMessage(3, nullptr, 0, size);
        return;
    }
    if (command == 2) {
        if (!download.valid) Com_Error(ERR_DROP, "CoD4x redirect without checksum metadata");
        if (cls.download) { FS_FCloseFile(cls.download); cls.download = 0; }
        CL_ParseWWWDownload(0, msg);
        return;
    }
    if (command != 0 || !download.valid || !cls.download)
        Com_Error(ERR_DROP, "Unexpected CoD4x download block");
    const int offset = MSG_ReadLong(msg);
    const unsigned length = unsigned(MSG_ReadShort(msg)) & 65535;
    if (msg->overflowed || offset != cls.downloadCount || length > unsigned(download.size - cls.downloadCount)
        || length > unsigned(msg->cursize - msg->readcount))
        Com_Error(ERR_DROP, "Invalid CoD4x download range");
    if (length) {
        if (FS_Write((char *)msg->data + msg->readcount, length, cls.download) != length)
            Com_Error(ERR_DROP, "Could not save CoD4x download (check free space)");
        cls.downloadCount += length; legacyHacks.cl_downloadCount = cls.downloadCount;
        return;
    }
    if (cls.downloadCount != download.size) Com_Error(ERR_DROP, "Incomplete CoD4x download");
    FS_FCloseFile(cls.download); cls.download = 0;
    char osPath[1024]; FS_BuildOSPath(Dvar_GetString("fs_homepath"), cls.downloadTempName, (char *)"", osPath);
    size_t n = strlen(osPath); if (n && osPath[n-1] == '/') osPath[n-1] = 0;
    if (!CL_CoD4xVerifyDownload(osPath)) Com_Error(ERR_DROP, "CoD4x download checksum mismatch");
    FS_SV_Rename(cls.downloadTempName, cls.downloadName);
    cls.downloadTempName[0] = cls.downloadName[0] = 0;
    CL_NextDownload(0);
}
}

bool CL_UsesCoD4x() { return cls.serverUsesExtendedConfigstrings && CL_ProtocolVersion() == 21; }
void CL_CoD4xRecordServerError(const char *reason) {
    if (CL_UsesCoD4x() && cod4x::AuthenticationRejected(reason)) {
        recordVerdict(true);
        Com_Printf(CON_CHANNEL_CLIENT, "CoD4x: authentication rejection saved for the server browser filter\n");
    }
}
void CL_CoD4xReset(bool preserveDownload) {
    started = false; steamReported = false; channel.reset(0); configSequence = 0;
    // A server can disconnect us while its HTTP redirect continues. Keep the expected
    // checksum until that transfer completes, even though the network channel is gone.
    if (!preserveDownload) download = {};
    CL_CoD4xClearGameState();
}
void CL_CoD4xStart() {
    CL_CoD4xReset(); channel.reset(uint16_t(CL_GetLocalClientConnection(0)->qport)); started = true;
    Com_Printf(CON_CHANNEL_CLIENT, "CoD4x reliable channel ready\n");
}
bool CL_CoD4xPacket(msg_t *msg) { return started && channel.receive(msg->data, msg->cursize); }
void CL_CoD4xRefreshServers() {
#ifdef __APPLE__
    if (!cl_hideSteamServers)
        cl_hideSteamServers = Dvar_RegisterBool("cl_hideSteamServers", 1, DVAR_ARCHIVE,
            "Show verified compatible servers; hide unknown servers and explicit authentication rejections");
    KisakMaster_Begin(); cls.waitglobalserverresponse = 1; cls.pingUpdateSource = 1;
#endif
}
void CL_CoD4xFrame(int localClientNum) {
#ifdef __APPLE__
    KisakMasterAddress addresses[8192];
    const int count = KisakMaster_Poll(addresses, 8192);
    if (count) {
        cls.waitglobalserverresponse = 0;
        if (count < 0) Com_Printf(CON_CHANNEL_CLIENT, "CoD4x master request failed; retaining cached servers\n");
        else {
            cls.numglobalservers = 0;
            int hidden = 0;
            for (int i = 0; i < count; ++i) {
                netadr_t address{}; address.type = NA_IP;
                memcpy(address.ip, addresses[i].ip, 4); memcpy(&address.port, addresses[i].port, 2);
                if (cl_hideSteamServers && cl_hideSteamServers->current.enabled && !worthShowing(address)) {
                    ++hidden;
                    continue;
                }
                CL_InitServerInfo(&cls.globalServers[cls.numglobalservers++], address);
            }
            CL_SortGlobalServers();
            Com_Printf(CON_CHANNEL_CLIENT, "CoD4x master: %d compatible candidates of %d listed (%d rejected or unverified)\n",
                cls.numglobalservers, count, hidden);
        }
    }
#endif
    if (!started || !CL_UsesCoD4x() || clientUIActives[localClientNum].connectionState < CA_CONNECTED) return;
    cod4x::ReliableTransport::Bytes bytes;
    while (channel.pop(bytes)) {
        msg_t msg; MSG_Init(&msg, bytes.data(), int(bytes.size())); msg.cursize = int(bytes.size());
        const int command = MSG_ReadLong(&msg);
        Com_DPrintf(CON_CHANNEL_CLIENT, "CoD4x reliable service %d, %d bytes\n", command, int(bytes.size()));
        switch (command) {
            case 1:
                if (MSG_ReadByte(&msg) != svc_gamestate) Com_Error(ERR_DROP, "Invalid reliable gamestate");
                Com_Printf(CON_CHANNEL_CLIENT, "CoD4x reliable gamestate: %d bytes\n", msg.cursize);
                recordVerdict(false);
                CL_ParseGamestate(localClientNum, &msg);
                break;
            case 9: sendStats(&msg); break;
            case 5: parseDownload(&msg); break;
            case 8: processSteamCommands(&msg); break;
            case 12: break; // svc_acdata: ignored, as in the public client

            default:
                Com_Printf(CON_CHANNEL_CLIENT, "CoD4x: unsupported reliable service %d\n", command);
                break;
        }
        if (msg.overflowed) {
            if (command == 1 || command == 5 || command == 9)
                Com_Error(ERR_DROP, "Truncated CoD4x reliable service %d", command);
            Com_Printf(CON_CHANNEL_CLIENT, "CoD4x: short optional reliable service %d ignored\n", command);
        }
        if (!started) return;
    }
    if (channel.failed()) Com_Error(ERR_DROP, "Invalid CoD4x reliable stream");
    for (auto &packet : channel.transmit(uint32_t(cls.realtime)))
        NET_SendPacket(NS_CLIENT1, int(packet.size()), packet.data(), CL_GetLocalClientConnection(localClientNum)->serverAddress);
}
int CL_CoD4xConfigSequence() { return configSequence; }
void CL_CoD4xSetConfigSequence(int sequence) {
    if (sequence < 0) Com_Error(ERR_DROP, "Invalid CoD4x config sequence");
    configSequence = sequence;
}
void CL_CoD4xStoreServerCommand(int sequence, const char *text) { commandHistory.store(sequence, text); }
char *CL_CoD4xServerCommand(int sequence) { return commandHistory.get(sequence); }
void CL_CoD4xClearGameState() {
    commandHistory.clear();
    for (auto &s : extendedStrings) s.clear();
    extendedBytes = 0; memset(remoteClients, 0, sizeof(remoteClients)); memset(clientValid, 0, sizeof(clientValid));
}
void CL_CoD4xSetConfigString(unsigned index, const char *value) {
    if (index < MAX_CONFIGSTRINGS || index >= 4884) Com_Error(ERR_DROP, "CoD4x configstring out of range");
    auto &s = extendedStrings[index - MAX_CONFIGSTRINGS];
    const size_t size = strlen(value);
    if (extendedBytes - s.size() + size > MAX_GAMESTATE_CHARS) Com_Error(ERR_DROP, "CoD4x extended gamestate overflow");
    extendedBytes = extendedBytes - s.size() + size; s = value;
}
const char *CL_CoD4xGetConfigString(unsigned index) {
    if (index < MAX_CONFIGSTRINGS || index >= 4884) Com_Error(ERR_DROP, "CoD4x configstring out of range");
    return extendedStrings[index - MAX_CONFIGSTRINGS].c_str();
}
void CL_CoD4xParseClient(msg_t *msg, bool gamestate) {
    const int sequence = gamestate ? configSequence : MSG_ReadLong(msg);
    const unsigned index = MSG_ReadByte(msg);
    ClientData data{};
    I_strncpyz(data.name, MSG_ReadString(msg), sizeof(data.name));
    I_strncpyz(data.clan, MSG_ReadString(msg), sizeof(data.clan));
    if (msg->overflowed || index >= 64) Com_Error(ERR_DROP, "Invalid CoD4x client data");
    if (!gamestate && sequence != configSequence + 1) return;
    remoteClients[index] = data; clientValid[index] = true;
    if (!gamestate) configSequence = sequence;
}
const char *CL_CoD4xClientName(int index) { return index >= 0 && index < 64 && clientValid[index] ? remoteClients[index].name : nullptr; }
void CL_CoD4xBeginDownload(const char *name) {
    // Only game assets below the writable game directory; never interpret remote paths as commands.
    if (!name || strlen(name) >= sizeof(download.name) || name[0] == '/' || strstr(name, "..") || strpbrk(name, "\\:\";\r\n"))
        Com_Error(ERR_DROP, "Invalid CoD4x download path");
    const char *extension = strrchr(name, '.');
    if (!extension || (I_stricmp(extension, ".ff") && I_stricmp(extension, ".iwd")))
        Com_Error(ERR_DROP, "Unsupported CoD4x download type");
    download = {}; I_strncpyz(download.name, name, sizeof(download.name)); downloadMessage(0, name);
}
bool CL_CoD4xDownloadCommand(const char *command) {
    if (!started || !CL_UsesCoD4x()) return false;
    if (!strcmp(command, "wwwdl ack")) return true; // inherent in reliable transport
    int subcommand = -1;
    if (!strcmp(command, "donedl")) subcommand = 1;
    else if (!strcmp(command, "wwwdl fail")) subcommand = 4;
    else if (!strcmp(command, "wwwdl done")) subcommand = 5;
    else if (!strcmp(command, "wwwdl bbl8r")) subcommand = 6;
    else if (!strcmp(command, "wwwdl chkfail")) subcommand = 7;
    if (subcommand < 0) return false;
    downloadMessage(subcommand); return true;
}
bool CL_CoD4xVerifyDownload(const char *osPath) {
    if (!download.name[0]) return !CL_UsesCoD4x();
    if (!download.valid) {
        Com_PrintError(CON_CHANNEL_CLIENT, "CoD4x verification missing metadata for %s\n", download.name);
        return false;
    }
    FILE *file = fopen(osPath, "rb");
    if (!file) {
        Com_PrintError(CON_CHANNEL_CLIENT, "CoD4x verification could not open %s\n", osPath);
        return false;
    }
    uint8_t bytes[65536]; uint32_t checksum = 0; size_t size = 0, n;
    while ((n = fread(bytes, 1, sizeof(bytes), file)) != 0) { checksum = crc32(checksum, bytes, unsigned(n)); size += n; }
    const bool okay = !ferror(file) && size == size_t(download.size) && checksum == download.checksum;
    if (!okay)
        Com_PrintError(CON_CHANNEL_CLIENT,
            "CoD4x verification %s: bytes %zu/%d CRC32 %08x/%08x read error %d\n",
            download.name, size, download.size, checksum, download.checksum, ferror(file));
    fclose(file); return okay;
}

void CL_CoD4xSendPureChecksums(int localClientNum) {
    // Protocol 21 inserts localization and the full server id before the IWD list.
    // Without them SV_VerifyPaks_f interprets an IWD checksum as the server id and
    // silently ignores the command, leaving the client primed indefinitely.
    std::string command = "cp @ L" + std::to_string(SEH_GetCurrentLanguage()) + " "
        + std::to_string(CL_GetLocalClientGlobals(localClientNum)->serverId) + " ";
    uint32_t checksum = uint32_t(fs_checksumFeed);
    unsigned count = 0;
    for (const searchpath_s *search = fs_searchpaths; search; search = search->next) {
        // CoD4x checks referenced localized archives as well as shared archives.
        if (!search->iwd || !search->iwd->referenced) continue;
        command += std::to_string(int32_t(search->iwd->pure_checksum)) + " ";
        checksum ^= uint32_t(search->iwd->pure_checksum);
        ++count;
    }
    extern int fs_fakeChkSum;
    if (fs_fakeChkSum) command += std::to_string(fs_fakeChkSum) + " ";
    command += std::to_string(int32_t(count ^ checksum));
    if (command.size() >= sizeof(CL_GetLocalClientConnection(localClientNum)->reliableCommands[0]))
        Com_Error(ERR_DROP, "CoD4x file verification command is too long");
    CL_AddReliableCommand(localClientNum, command.c_str());
    Com_Printf(CON_CHANNEL_CLIENT, "CoD4x file verification: %u archives, server id %d\n",
        count, CL_GetLocalClientGlobals(localClientNum)->serverId);
}
