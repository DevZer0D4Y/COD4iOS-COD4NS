#pragma once
#include <array>
#include <cstring>
#include <string>
namespace cod4x {
// The network still uses its 128-command ring. Keep a bounded presentation history
// for iOS resume/frame stalls, where cgame may consume commands after that ring wraps.
template <size_t Capacity = 2048> class CommandHistory {
    // CoD4x accepts server command payloads up to 8191 bytes. Allocate only
    // the received text rather than reserving 16 MB for mostly empty entries.
    struct Entry { int sequence = -1; std::string text; };
    std::array<Entry, Capacity> entries{};
public:
    void clear() { for (auto &entry : entries) { entry.sequence = -1; std::string{}.swap(entry.text); } }
    void store(int sequence, const char *text) {
        if (sequence < 0 || !text) return;
        auto &entry = entries[static_cast<unsigned>(sequence) % Capacity];
        entry.sequence = sequence;
        entry.text.assign(text, strnlen(text, 8191));
    }
    char *get(int sequence) {
        if (sequence < 0) return nullptr;
        auto &entry = entries[static_cast<unsigned>(sequence) % Capacity];
        return entry.sequence == sequence ? entry.text.data() : nullptr;
    }
};
}
