// UDP networking for the Apple port's multiplayer build, replacing src/win32/win_net.cpp.
//
// Single-player never opens a socket, so this only matters for KISAK_MP: the engine hands packets
// to Sys_SendPacket and polls Sys_GetPacket, and both server and client run over one bound UDP
// socket. BSD sockets map one-to-one onto the Winsock calls the original used; the differences are
// the error codes (errno rather than WSAGetLastError) and non-blocking mode (O_NONBLOCK).
//
// Voice chat (Voice_*) has no implementation here: CoD4's voice runs over Speex through the same
// channel, and the port has no capture device wired up, so those entry points are stubs.

#include <universal/q_shared.h>
#include <qcommon/qcommon.h>
#include <qcommon/net_chan_mp.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

int s_ipSocket = -1;
int s_serverSocket = -1;

void NetadrToSockadr(const netadr_t &address, sockaddr_in &out)
{
    memset(&out, 0, sizeof(out));
    out.sin_family = AF_INET;
    out.sin_len = sizeof(out);
    if (address.type == NA_BROADCAST)
    {
        out.sin_addr.s_addr = INADDR_BROADCAST;
    }
    else
    {
        memcpy(&out.sin_addr.s_addr, address.ip, 4);
    }
    out.sin_port = address.port; // already network order, as in the original
}

void SockadrToNetadr(const sockaddr_in &address, netadr_t &out)
{
    memset(&out, 0, sizeof(out));
    out.type = NA_IP;
    memcpy(out.ip, &address.sin_addr.s_addr, 4);
    out.port = address.sin_port;
}

int OpenSocket(uint16_t port, const char *label)
{
    const int handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (handle < 0)
    {
        Com_PrintError(CON_CHANNEL_SYSTEM, "NET: socket() failed for %s: %s\n", label, strerror(errno));
        return -1;
    }
    int flags = fcntl(handle, F_GETFL, 0);
    if (flags < 0 || fcntl(handle, F_SETFL, flags | O_NONBLOCK) < 0)
        Com_PrintWarning(CON_CHANNEL_SYSTEM, "NET: could not make %s socket non-blocking: %s\n", label, strerror(errno));
    int enable = 1;
    setsockopt(handle, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable));
    setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_len = sizeof(address);
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(handle, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
    {
        Com_PrintError(CON_CHANNEL_SYSTEM, "NET: bind() to port %u failed for %s: %s\n", port, label, strerror(errno));
        close(handle);
        return -1;
    }
    Com_Printf(CON_CHANNEL_SYSTEM, "NET: %s socket bound to port %u\n", label, port);
    return handle;
}

} // namespace

int __cdecl Sys_GetPacket(netadr_t *net_from, msg_t *net_message)
{
    for (int which = 0; which < 2; ++which)
    {
        const int handle = which == 0 ? s_ipSocket : s_serverSocket;
        if (handle < 0)
            continue;
        sockaddr_in from = {};
        socklen_t fromLength = sizeof(from);
        const ssize_t received = recvfrom(handle, net_message->data, net_message->maxsize, 0,
                                          reinterpret_cast<sockaddr *>(&from), &fromLength);
        if (received < 0)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != ECONNREFUSED)
                Com_PrintWarning(CON_CHANNEL_SYSTEM, "NET: recvfrom failed: %s\n", strerror(errno));
            continue;
        }
            SockadrToNetadr(from, *net_from);
        net_message->readcount = 0;
        if (received == net_message->maxsize)
        {
            Com_PrintWarning(CON_CHANNEL_SYSTEM, "NET: oversize packet from %s\n", NET_AdrToString(*net_from));
            continue;
        }
        net_message->cursize = static_cast<int>(received);
        return 1;
    }
    return 0;
}

char __cdecl Sys_SendPacket(int length, unsigned char *data, netadr_t to)
{
    if (to.type != NA_IP && to.type != NA_BROADCAST)
    {
        Com_PrintError(CON_CHANNEL_SYSTEM, "Sys_SendPacket: bad address type %d\n", (int)to.type);
        return 0;
    }
    const int handle = s_ipSocket >= 0 ? s_ipSocket : s_serverSocket;
    if (handle < 0)
        return 0;

    sockaddr_in address;
    NetadrToSockadr(to, address);
    const ssize_t sent = sendto(handle, data, static_cast<size_t>(length), 0,
                                reinterpret_cast<sockaddr *>(&address), sizeof(address));
    if (sent >= 0)
        return 1;
    // A refused datagram just means nothing is listening yet; the engine retries.
    if (errno != ECONNREFUSED && errno != EAGAIN && errno != EWOULDBLOCK)
        Com_PrintError(CON_CHANNEL_SYSTEM, "Sys_SendPacket: %s\n", strerror(errno));
    return 0;
}

void Sys_ShowIP()
{
    ifaddrs *interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0 || !interfaces)
    {
        Com_Printf(CON_CHANNEL_SYSTEM, "NET: no network interfaces found\n");
        return;
    }
    for (ifaddrs *entry = interfaces; entry; entry = entry->ifa_next)
    {
        if (!entry->ifa_addr || entry->ifa_addr->sa_family != AF_INET)
            continue;
        if ((entry->ifa_flags & IFF_UP) == 0 || (entry->ifa_flags & IFF_LOOPBACK) != 0)
            continue;
        char text[INET_ADDRSTRLEN] = {};
        const sockaddr_in *address = reinterpret_cast<const sockaddr_in *>(entry->ifa_addr);
        inet_ntop(AF_INET, &address->sin_addr, text, sizeof(text));
        Com_Printf(CON_CHANNEL_SYSTEM, "IP: %s (%s)\n", text, entry->ifa_name);
    }
    freeifaddrs(interfaces);
}

// "1.2.3.4", "1.2.3.4:28960", "localhost" or a host name, as the original accepted.
bool __cdecl Sys_StringToAdr(const char *text, netadr_t *address)
{
    if (!text || !address)
        return false;
    memset(address, 0, sizeof(*address));

    char host[256] = {};
    I_strncpyz(host, text, sizeof(host));
    const char *service = nullptr;
    if (char *colon = strchr(host, ':'))
    {
        *colon = 0;
        service = colon + 1;
    }
    if (!I_stricmp(host, "localhost"))
    {
        address->type = NA_LOOPBACK;
        return true;
    }

    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo *results = nullptr;
    if (getaddrinfo(host, service, &hints, &results) != 0 || !results)
        return false;
    const sockaddr_in *resolved = reinterpret_cast<const sockaddr_in *>(results->ai_addr);
    SockadrToNetadr(*resolved, *address);
    if (!service)
        address->port = 0;
    freeaddrinfo(results);
    return true;
}

void __cdecl Sys_ShowConsole()
{
    // No separate console window on this platform; output goes to the log.
}

void __cdecl Sys_QuitAndStartProcess(const char *exeName, const char *parameters)
{
    // The Windows build uses this to hand off between the SP and MP executables. Each is its
    // own app here, so there is nothing to launch.
    Com_Printf(CON_CHANNEL_SYSTEM, "Sys_QuitAndStartProcess ignored: %s %s\n", exeName ? exeName : "",
               parameters ? parameters : "");
}

void Sys_InitNetworking(uint16_t clientPort, uint16_t serverPort)
{
    if (s_ipSocket < 0)
        s_ipSocket = OpenSocket(clientPort, "client");
    if (serverPort && s_serverSocket < 0 && serverPort != clientPort)
        s_serverSocket = OpenSocket(serverPort, "server");
}

void Sys_ShutdownNetworking()
{
    if (s_ipSocket >= 0)
        close(s_ipSocket);
    if (s_serverSocket >= 0)
        close(s_serverSocket);
    s_ipSocket = -1;
    s_serverSocket = -1;
}

bool Sys_IsLANAddress(netadr_t adr)
{
    if (adr.type == NA_LOOPBACK)
        return true;
    if (adr.type != NA_IP)
        return false;
    // The usual private ranges, matching the original's table.
    if (adr.ip[0] == 10 || adr.ip[0] == 127)
        return true;
    if (adr.ip[0] == 172 && adr.ip[1] >= 16 && adr.ip[1] <= 31)
        return true;
    if (adr.ip[0] == 192 && adr.ip[1] == 168)
        return true;
    return false;
}

bool Sys_IsLANAddress_IgnoreSubnet(netadr_t adr)
{
    return Sys_IsLANAddress(adr);
}

// Voice chat: no capture device is wired up on this platform.
void Voice_Init() {}
void Voice_Shutdown() {}
void Voice_Playback() {}
double __cdecl Voice_GetVoiceLevel() { return 0.0; }
int __cdecl Voice_GetLocalVoiceData() { return 0; }
bool __cdecl Voice_IsClientTalking(uint32_t clientNum) { (void)clientNum; return false; }
void __cdecl Voice_IncomingVoiceData(unsigned char clientNum, unsigned char *data, int dataSize)
{
    (void)clientNum; (void)data; (void)dataSize;
}

void IN_SetCursorPos(POINT position)
{
    (void)position; // the port drives the cursor itself (apple_sys.cpp)
}
