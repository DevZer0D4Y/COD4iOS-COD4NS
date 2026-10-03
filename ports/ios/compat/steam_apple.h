// Steam client authentication is a Windows-only part of the multiplayer client (win_steam.cpp).
// The Apple port has no Steam runtime, so these stubs report "no ticket": the client then
// connects without a Steam auth ticket, which is what unofficial servers expect.
#pragma once

#include <cstdint>
#include <qcommon/net_chan_mp.h> // netadr_t

void Steam_Init();
void Steam_Shutdown();
bool Steam_UpdateClientAuthTicket(netadr_t serverIpv4);
bool Steam_GetRawClientTicket(unsigned char **pBuffer, uint32_t *pSize);
void Steam_CancelClientTicket();
uint64_t Steam_GetClientSteamID64();

// Dedicated-server side: with no Steam runtime every ticket check simply passes, which is how
// unofficial/LAN servers already run.
bool Steam_CheckClientTicket(const void *pAuthTicket, uint32_t authTicketLen, uint64_t steamID64);
void Steam_CheckClients();
void Steam_OnClientDropped(uint64_t steamID64);
void Steam_SV_AddTestCommands();
