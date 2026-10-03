#include "cod4x_transport.h"
#include <algorithm>
#include <limits>

namespace cod4x {
namespace {
void put(ReliableTransport::Bytes &b, uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) b.push_back(uint8_t(value >> (i * 8)));
}
uint32_t get(const uint8_t *b, size_t offset, int bytes) {
    uint32_t value = 0;
    for (int i = 0; i < bytes; ++i) value |= uint32_t(b[offset + i]) << (i * 8);
    return value;
}
}
void ReliableTransport::reset(uint16_t qport) {
    qport_ = qport; rxBase_ = txBase_ = txNext_ = 0;
    for (auto &f : rx_) f = Fragment{};
    tx_.clear(); stream_.clear(); lastAck_ = 0; ackNeeded_ = true; failed_ = false;
}
bool ReliableTransport::receive(const uint8_t *b, size_t size) {
    if (failed_ || !b || size < 20 || get(b, 0, 4) != 0xfffffff0u || get(b, 4, 2) != qport_) return false;
    const int32_t sequence = int32_t(get(b, 6, 4));
    const int32_t ack = int32_t(get(b, 10, 4));
    const uint32_t count = b[15];
    if (count > 3 || size < 20 + count * 4 || sequence < -1 || ack < 0 || ack > txNext_) return false;
    const size_t header = 20 + count * 4;
    const size_t length = get(b, header - 2, 2);
    if (length > FragmentSize || size != header + length || (sequence == -1 && length)) return false;
    std::array<std::pair<int32_t, int32_t>, 3> sacks{};
    for (uint32_t i = 0; i < count; ++i) {
        const int64_t begin = int64_t(ack) + get(b, 16 + i * 4, 2);
        const int64_t end = begin + get(b, 18 + i * 4, 2);
        if (end > txNext_ || begin < ack) return false;
        sacks[i] = {int32_t(begin), int32_t(end)};
    }
    // Delayed ACKs must not suppress a useful receive fragment or move our send window backwards.
    while (!tx_.empty() && tx_.front().sequence < ack) tx_.pop_front();
    txBase_ = std::max(txBase_, ack);
    for (auto &f : tx_) for (uint32_t i = 0; i < count; ++i)
        if (f.sequence >= sacks[i].first && f.sequence < sacks[i].second) f.ack = true;
    if (sequence == -1) return true;
    ackNeeded_ = true;
    if (sequence < rxBase_) return true;
    if (int64_t(sequence) >= int64_t(rxBase_) + 20) return false;
    auto &slot = rx_[size_t(sequence) % rx_.size()];
    if (slot.sequence != sequence) {
        slot.sequence = sequence;
        slot.bytes.assign(b + header, b + size);
    }
    while (rx_[size_t(rxBase_) % rx_.size()].sequence == rxBase_) {
        auto &next = rx_[size_t(rxBase_) % rx_.size()];
        if (stream_.size() + next.bytes.size() > 2 * (MaxMessage + 4) || rxBase_ == std::numeric_limits<int32_t>::max()) {
            fail(); return false;
        }
        stream_.insert(stream_.end(), next.bytes.begin(), next.bytes.end());
        next = Fragment{};
        ++rxBase_;
    }
    return true;
}
bool ReliableTransport::send(const uint8_t *message, size_t size) {
    if (failed_ || !message || size < 4 || size > MaxMessage || tx_.size() * FragmentSize + size + 4 > 2 * (MaxMessage + 4)) return false;
    Bytes framed; framed.reserve(size + 4); put(framed, uint32_t(size), 4);
    framed.insert(framed.end(), message, message + size);
    for (size_t offset = 0; offset < framed.size(); offset += FragmentSize) {
        if (txNext_ == std::numeric_limits<int32_t>::max()) { fail(); return false; }
        Fragment f; f.sequence = txNext_++;
        f.bytes.assign(framed.begin() + offset, framed.begin() + std::min(offset + FragmentSize, framed.size()));
        tx_.push_back(std::move(f));
    }
    return true;
}
ReliableTransport::Bytes ReliableTransport::packet(int32_t sequence, const Bytes &payload) const {
    Bytes b; b.reserve(1400);
    put(b, 0xfffffff0u, 4); put(b, qport_, 2); put(b, uint32_t(sequence), 4); put(b, uint32_t(rxBase_), 4);
    put(b, 0, 1); put(b, 0, 1);
    unsigned ranges = 0;
    for (int i = 1; i < 20 && ranges < 3;) {
        if (rx_[size_t(rxBase_ + i) % rx_.size()].sequence != rxBase_ + i) { ++i; continue; }
        const int start = i++;
        while (i < 20 && rx_[size_t(rxBase_ + i) % rx_.size()].sequence == rxBase_ + i) ++i;
        put(b, start, 2); put(b, i - start, 2); ++ranges;
    }
    b[15] = uint8_t(ranges);
    put(b, 4, 2); put(b, uint32_t(payload.size()), 2);
    b.insert(b.end(), payload.begin(), payload.end());
    return b;
}
std::vector<ReliableTransport::Bytes> ReliableTransport::transmit(uint32_t now) {
    std::vector<Bytes> out;
    if (failed_) return out;
    for (auto &f : tx_) {
        if (int64_t(f.sequence) >= int64_t(txBase_) + 4) break;
        if (!f.ack && (!f.sentOnce || uint32_t(now - f.sent) >= 350)) {
            out.push_back(packet(f.sequence, f.bytes)); f.sent = now; f.sentOnce = true;
        }
    }
    if (out.empty() && (ackNeeded_ || uint32_t(now - lastAck_) >= 350)) out.push_back(packet(-1, {}));
    if (!out.empty()) { ackNeeded_ = false; lastAck_ = now; }
    return out;
}
bool ReliableTransport::pop(Bytes &message) {
    if (failed_ || stream_.size() < 4) return false;
    uint32_t size = 0;
    for (int i = 0; i < 4; ++i) size |= uint32_t(stream_[i]) << (8 * i);
    if (size < 4 || size > MaxMessage) { fail(); return false; }
    if (stream_.size() < size + 4) return false;
    for (int i = 0; i < 4; ++i) stream_.pop_front();
    message.resize(size);
    for (auto &b : message) { b = stream_.front(); stream_.pop_front(); }
    return true;
}
}
