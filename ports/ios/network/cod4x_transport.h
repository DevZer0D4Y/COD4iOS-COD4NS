#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

// CoD4x's reliable UDP stream, independent of the engine and platform sockets.
// Wire reference: callofduty4x/CoD4x_Client_pub, net_reliabletransport.c.
// All multibyte values are little endian. Messages in the stream have a 32-bit length prefix.
namespace cod4x {
class ReliableTransport {
public:
    using Bytes = std::vector<uint8_t>;
    static constexpr size_t FragmentSize = 1200;
    static constexpr size_t MaxMessage = 1024 * 1024 - 4;
    void reset(uint16_t qport);
    bool receive(const uint8_t *packet, size_t size);
    bool send(const uint8_t *message, size_t size);
    std::vector<Bytes> transmit(uint32_t now);
    // Returns false until one whole message is available. Malformed stream closes the channel.
    bool pop(Bytes &message);
    bool failed() const { return failed_; }
private:
    struct Fragment { int32_t sequence = -1; Bytes bytes; uint32_t sent = 0; bool sentOnce = false; bool ack = false; };
    Bytes packet(int32_t sequence, const Bytes &payload) const;
    void fail() { failed_ = true; }
    uint16_t qport_ = 0;
    int32_t rxBase_ = 0, txBase_ = 0, txNext_ = 0;
    std::array<Fragment, 32> rx_{};
    std::deque<Fragment> tx_;
    std::deque<uint8_t> stream_;
    uint32_t lastAck_ = 0;
    bool ackNeeded_ = true, failed_ = false;
};
}
