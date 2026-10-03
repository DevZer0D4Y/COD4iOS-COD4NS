#include "cod4x_transport.h"
#include <cassert>
#include <cstdio>
using cod4x::ReliableTransport;
using Bytes = ReliableTransport::Bytes;
int main() {
    ReliableTransport a, b;
    a.reset(123); b.reset(123);
    // Literal upstream-format packet: sequence 0, ack 0, no SACK, window 4,
    // eight stream bytes (length 4 + svc_statscommands).
    const uint8_t fixture[] = {240,255,255,255,123,0,0,0,0,0,0,0,0,0,0,0,4,0,8,0,4,0,0,0,9,0,0,0};
    assert(b.receive(fixture, sizeof(fixture)));
    Bytes message;
    assert(b.pop(message) && message == Bytes({9,0,0,0}));
    assert(!b.pop(message));
    assert(b.receive(fixture, sizeof(fixture))); // duplicate is not delivered twice
    assert(!b.pop(message));
    for (size_t n = 0; n < sizeof(fixture); ++n) assert(!a.receive(fixture, n));
    auto bad = Bytes(fixture, fixture + sizeof(fixture)); bad[4] = 1;
    assert(!a.receive(bad.data(), bad.size()));
    bad = Bytes(fixture, fixture + sizeof(fixture)); bad[15] = 255;
    assert(!a.receive(bad.data(), bad.size()));
    a.reset(123); b.reset(123);
    Bytes large(80017); for (size_t i = 0; i < large.size(); ++i) large[i] = uint8_t(i * 17);
    assert(a.send(large.data(), large.size()));
    Bytes reply{5,0,0,0,2,7,9}; assert(b.send(reply.data(), reply.size()));
    int receivedA = 0, receivedB = 0;
    uint32_t rng = 8191;
    auto lose = [&]() { rng = rng * 1664525u + 1013904223u; return rng % 7 == 0; };
    for (uint32_t t = 0; t < 60000 && (!receivedA || !receivedB); t += 50) {
        auto outgoing = a.transmit(t);
        // Reverse each burst, lose selected datagrams including the first fragment, duplicate others.
        for (size_t i = outgoing.size(); i > 0; --i) {
            auto &p = outgoing[i - 1];
            if (t == 0 && i == 1) continue;
            if (lose()) continue;
            assert(b.receive(p.data(), p.size()));
            assert(b.receive(p.data(), p.size()));
        }
        for (auto &p : b.transmit(t)) if (!lose()) assert(a.receive(p.data(), p.size()));
        while (a.pop(message)) { assert(message == reply); ++receivedA; }
        while (b.pop(message)) { assert(message == large); ++receivedB; }
        assert(!a.failed() && !b.failed());
    }
    assert(receivedA == 1 && receivedB == 1);
    // Reconnect discards fragments and messages from the previous channel.
    b.reset(456); assert(!b.receive(fixture, sizeof(fixture))); assert(!b.pop(message));
    a.reset(123); b.reset(123);
    bad = Bytes(fixture, fixture + sizeof(fixture)); bad[20] = 255; bad[21] = 255; bad[22] = 255; bad[23] = 127;
    assert(b.receive(bad.data(), bad.size())); assert(!b.pop(message) && b.failed());
    std::puts("CoD4x reliable transport: wire fixture, loss, reordering, duplicates, bidirectional transfer, bounds and reconnect passed");
}
