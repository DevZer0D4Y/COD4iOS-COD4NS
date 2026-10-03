#include "cod4x_server_filter.h"
#include "cod4x_verified_servers.h"
#include <cassert>
#include <cstdio>
int main() {
    using namespace cod4x;
    constexpr int64_t now=1800000000;
    assert(VerifiedServer(0x3624b1f0u,28930)); // Independently verified APG endpoint.
    assert(!VerifiedServer(0xa3b0c796u,28961)); // Evolution did not supply a gamestate.
    assert(!VerifiedServer(0x3624b1f0u,28931)); // Port is part of the identity.
    assert(ShowServer(true,nullptr,now));  // Clean install has verified candidates.
    assert(!ShowServer(false,nullptr,now)); // Clean install never falls back to all.
    ServerVerdict blocked{1,28960,true,now};
    assert(!ShowServer(true,&blocked,now)); // Explicit rejection overrides baseline.
    ServerVerdict allowed{2,28960,false,now};
    assert(ShowServer(false,&allowed,now)); // Successful direct join adds a server.
    assert(ShowServer(true,&blocked,now+ServerVerdictLifetime)); // Retry stale rejection.
    assert(!ShowServer(false,&allowed,now+ServerVerdictLifetime));
    allowed.when=now+1;assert(!ShowServer(false,&allowed,now)); // Future/bad cache date.
    assert(AuthenticationRejected("Authorization failed to complete within the timeout limit."));
    assert(AuthenticationRejected("STEAM AUTHENTICATION REQUIRED"));
    assert(AuthenticationRejected("This server requires Steam authentication."));
    assert(!AuthenticationRejected("Server asked for Steam authentication"));
    assert(!AuthenticationRejected("EXE_ERR_SERVER_TIMEOUT"));
    assert(!AuthenticationRejected("Player kicked by scriptadmin"));
    assert(!AuthenticationRejected("cod4x_patchv2.ff is different from the server"));
    assert(!AuthenticationRejected(nullptr));
    puts("Fresh-install server filter, local overrides, expiry and auth rejection tests passed");
}
