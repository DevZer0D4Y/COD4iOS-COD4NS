#include "apple_master.h"
#include <future>
#include <vector>
#include <cstring>
#include <chrono>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <algorithm>
namespace {
std::future<std::vector<KisakMasterAddress>> query;
std::vector<KisakMasterAddress> fetch() {
    std::vector<KisakMasterAddress> result;
    addrinfo hints{}, *addresses = nullptr; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo("cod4master.cod4x.ovh", "20810", &hints, &addresses)) return result;
    int fd = -1;
    for (auto *a = addresses; a; a = a->ai_next) {
        fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol); if (fd < 0) continue;
        const int yes = 1; setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
        fcntl(fd, F_SETFL, O_NONBLOCK);
        connect(fd, a->ai_addr, a->ai_addrlen);
        pollfd p{fd, POLLOUT, 0}; int error = 0; socklen_t n = sizeof(error);
        if (poll(&p, 1, 5000) > 0 && getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &n) == 0 && !error) break;
        close(fd); fd = -1;
    }
    freeaddrinfo(addresses); if (fd < 0) return result;
    const uint8_t command[] = "\xff\xff\xff\xffgetservers 21 full empty";
    size_t sent = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (sent < sizeof(command) && std::chrono::steady_clock::now() < deadline) {
        pollfd p{fd, POLLOUT, 0}; if (poll(&p, 1, 500) <= 0) continue;
        ssize_t n = ::send(fd, command + sent, sizeof(command) - sent, 0); if (n <= 0) { close(fd); return result; } sent += size_t(n);
    }
    std::vector<uint8_t> bytes; bool complete = false;
    while (bytes.size() < 256 * 1024 && std::chrono::steady_clock::now() < deadline) {
        pollfd p{fd, POLLIN, 0}; if (poll(&p, 1, 500) <= 0) continue;
        uint8_t buffer[8192]; ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
        if (n == 0) { complete = true; break; } if (n < 0) break;
        bytes.insert(bytes.end(), buffer, buffer + n);
    }
    close(fd);
    const char header[] = "\xff\xff\xff\xffgetserversResponse";
    if (!complete || bytes.size() < sizeof(header) || memcmp(bytes.data(), header, sizeof(header) - 1)) return result;
    size_t pos = sizeof(header) - 1;
    while (pos < bytes.size() && bytes[pos] == '\\') {
        // TCP master records: delimiter, 4-byte id, up to three typed addresses.
        if (pos + 4 <= bytes.size() && (!memcmp(bytes.data() + pos, "\\EOT", 4) || !memcmp(bytes.data() + pos, "\\EOF", 4))) break;
        if (bytes.size() - pos < 6) return {};
        pos += 5;
        for (int i = 0; i < 3; ++i) {
            if (pos == bytes.size()) return {};
            const int type = bytes[pos++]; const size_t size = type == 4 ? 4 : type == 5 ? 16 : 0;
            if (!size || bytes.size() - pos < size + 2) return {};
            if (type == 4) {
                KisakMasterAddress address; memcpy(address.ip, bytes.data() + pos, 4); memcpy(address.port, bytes.data() + pos + 4, 2);
                result.push_back(address);
            }
            pos += size + 2;
            if (pos == bytes.size() || bytes[pos] == '\\') break;
        }
    }
    return result;
}
}
void KisakMaster_Begin() {
    if (query.valid()) return;
    query = std::async(std::launch::async, fetch);
}
int KisakMaster_Poll(KisakMasterAddress *addresses, int capacity) {
    if (!query.valid() || query.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return 0;
    auto result = query.get();
    if (result.empty()) return -1;
    const int count = std::min(capacity, int(result.size()));
    memcpy(addresses, result.data(), size_t(count) * sizeof(*addresses)); return count;
}
