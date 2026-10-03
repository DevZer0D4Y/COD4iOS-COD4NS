#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace kisakcod::assets {

struct BspVertex {
    std::array<float, 3> position{};
    std::array<float, 3> normal{};
    std::array<float, 4> color{}; // RGBA, [0,1]; disk BGRA is converted.
};

struct PlayerSpawn {
    std::array<float, 3> position{};
    std::array<float, 3> angles{}; // Pitch, yaw, roll in degrees.
};

struct BspMesh {
    std::vector<BspVertex> vertices;
    std::vector<std::uint32_t> indices; // Absolute indices; triangle-list order.
    std::array<float, 3> minBounds{};
    std::array<float, 3> maxBounds{};
    std::optional<PlayerSpawn> spawn;
    std::uint32_t surfaceCount = 0;
    bool layered = false;
};

class BspError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Limits can be lowered by callers. Larger values are clamped to these defaults.
struct BspLoadLimits {
    std::size_t maxFileBytes = 128u * 1024u * 1024u;
    std::size_t maxVertices = 1'000'000;
    std::size_t maxIndices = 6'000'000;
    std::size_t maxSurfaces = 65'536;
    std::size_t maxEntityBytes = 4u * 1024u * 1024u;
};

// Loads loose little-endian IBSP v22 only; not .ff or .iwd containers.
// Prefers the complete unlayered representation when it has surfaces, matching
// the upstream simple-material mode, and otherwise selects layered geometry.
// Coordinates retain COD4's native X/Y ground plane and Z-up convention.
// Materials, textures, models, collision and entity execution are not loaded.
// Malformed input and budget violations throw BspError with an actionable reason.
BspMesh loadBsp(const std::vector<std::uint8_t>& bytes,
                const BspLoadLimits& limits = {});
BspMesh loadBsp(const std::string& path, const BspLoadLimits& limits = {});
// Reused by the fastfile reader after validating the serialized MapEnts record.
std::optional<PlayerSpawn> parseBspSpawn(std::string_view entityText);

} // namespace kisakcod::assets
