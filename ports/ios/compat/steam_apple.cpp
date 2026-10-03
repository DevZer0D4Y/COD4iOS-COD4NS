// See steam_apple.h: no Steam runtime on this platform.
#include <universal/q_shared.h>
#include <qcommon/qcommon.h>
#include "steam_apple.h"

void Steam_Init() {}
void Steam_Shutdown() {}

bool Steam_UpdateClientAuthTicket(netadr_t serverIpv4)
{
    (void)serverIpv4;
    // CL_CDKeyValidate treats false as "invalid CD key" and refuses to connect at all. There is
    // no Steam runtime here, so report success and send no ticket: unofficial servers (the only
    // ones still online) accept clients without one.
    return true;
}

bool Steam_GetRawClientTicket(unsigned char **pBuffer, uint32_t *pSize)
{
    if (pBuffer)
        *pBuffer = nullptr;
    if (pSize)
        *pSize = 0;
    return false;
}

void Steam_CancelClientTicket() {}

uint64_t Steam_GetClientSteamID64()
{
    return 0;
}

bool Steam_CheckClientTicket(const void *pAuthTicket, uint32_t authTicketLen, uint64_t steamID64)
{
    (void)pAuthTicket; (void)authTicketLen; (void)steamID64;
    return true; // nothing to validate against
}

void Steam_CheckClients() {}

void Steam_OnClientDropped(uint64_t steamID64)
{
    (void)steamID64;
}

void Steam_SV_AddTestCommands() {}
