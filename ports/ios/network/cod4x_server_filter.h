#pragma once
#include <cstdint>
#include <cctype>
#include <string>

namespace cod4x {
constexpr int64_t ServerVerdictLifetime = 14 * 24 * 60 * 60;
struct ServerVerdict { uint32_t ip; uint16_t port; bool steam; int64_t when; };

// A fresh install starts from independently verified public server addresses.
// Recent local outcomes override that baseline; unknown addresses stay hidden
// while filtering is enabled. No player identity or connection history is bundled.
inline bool ShowServer(bool verifiedBaseline, const ServerVerdict *local, int64_t now) {
    if (local && local->when <= now && now - local->when < ServerVerdictLifetime)
        return !local->steam;
    return verifiedBaseline;
}

// A Steam request by itself, a generic network timeout or an admin kick does not
// establish incompatibility. Only explicit authentication rejection is cached.
inline bool AuthenticationRejected(const char *reason) {
    if (!reason) return false;
    std::string lower(reason);
    for (char &c : lower) c=char(std::tolower(static_cast<unsigned char>(c)));
    return lower.find("authorization failed to complete within the timeout limit") != std::string::npos
        || lower.find("steam authentication required") != std::string::npos
        || lower.find("requires steam authentication") != std::string::npos
        || lower.find("official cod4x client required") != std::string::npos;
}
}
