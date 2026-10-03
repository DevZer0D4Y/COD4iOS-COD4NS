#pragma once
#include <cstdint>
// Public server endpoints independently probed on 3 October 2026 using new,
// random test identities. Each returned a CoD4x gamestate without Steam.
// This is a compatibility baseline, not a player's saved server history.
// Local explicit rejections override the baseline through cod4x_server_filter.h.
namespace cod4x {
struct VerifiedServerAddress { uint32_t ip; uint16_t port; };
inline constexpr VerifiedServerAddress VerifiedServers[] = {
    {0x930f1518u, 28962}, // 147.15.21.24:28962
    {0x930f1518u, 28963}, // 147.15.21.24:28963
    {0x930f1518u, 28964}, // 147.15.21.24:28964
    {0x930f1518u, 28965}, // 147.15.21.24:28965
    {0x930f1518u, 28966}, // 147.15.21.24:28966
    {0x930f1518u, 28967}, // 147.15.21.24:28967
    {0x930f1518u, 28969}, // 147.15.21.24:28969
    {0x930f1518u, 28970}, // 147.15.21.24:28970
    {0x930f1518u, 28971}, // 147.15.21.24:28971
    {0x930f1518u, 28972}, // 147.15.21.24:28972
    {0x930f1518u, 28974}, // 147.15.21.24:28974
    {0x930f1518u, 28975}, // 147.15.21.24:28975
    {0x96881410u, 28960}, // 150.136.20.16:28960
    {0xb3ed613bu, 28960}, // 179.237.97.59:28960
    {0xd8ee435cu, 28960}, // 216.238.67.92:28960
    {0xd9b6adb4u, 28960}, // 217.182.173.180:28960
    {0xd9b6adb4u, 28962}, // 217.182.173.180:28962
    {0xd9b6adb4u, 28964}, // 217.182.173.180:28964
    {0x1f14c16eu, 28100}, // 31.20.193.110:28100
    {0x2ba5c673u, 28960}, // 43.165.198.115:28960
    {0x33ffeb93u, 28961}, // 51.255.235.147:28961
    {0x3624b1f0u, 28930}, // 54.36.177.240:28930
    {0x4087f0a1u, 28930}, // 64.135.240.161:28930
    {0x4ad0c8f0u, 28960}, // 74.208.200.240:28960
    {0x4ad0c8f0u, 28961}, // 74.208.200.240:28961
    {0x59a7263eu, 28960}, // 89.167.38.62:28960
};
inline bool VerifiedServer(uint32_t ip, uint16_t port) {
    for (const auto &server : VerifiedServers)
        if (server.ip==ip && server.port==port) return true;
    return false;
}
}
