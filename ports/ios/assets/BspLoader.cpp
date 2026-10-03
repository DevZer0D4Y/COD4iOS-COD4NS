#include "BspLoader.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <locale>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace kisakcod::assets {
namespace {

constexpr std::uint32_t kTriangles = 0x09;
constexpr std::uint32_t kVertices = 0x0a;
constexpr std::uint32_t kIndices = 0x0b;
constexpr std::uint32_t kEntities = 0x27;
constexpr std::uint32_t kSimpleTriangles = 0x2f;
constexpr std::uint32_t kSimpleVertices = 0x30;
constexpr std::uint32_t kSimpleIndices = 0x31;
constexpr std::size_t kVertexStride = 68;
constexpr std::size_t kSurfaceStride = 24;
constexpr std::size_t kMaxChunks = 100;
constexpr float kMaxCoordinate = 10'000'000.0f;

[[noreturn]] void fail(const std::string& reason) {
    throw BspError("IBSP: " + reason);
}

BspLoadLimits boundedLimits(const BspLoadLimits& requested) {
    const BspLoadLimits hard;
    return {std::min(hard.maxFileBytes, requested.maxFileBytes),
            std::min(hard.maxVertices, requested.maxVertices),
            std::min(hard.maxIndices, requested.maxIndices),
            std::min(hard.maxSurfaces, requested.maxSurfaces),
            std::min(hard.maxEntityBytes, requested.maxEntityBytes)};
}

class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t>& data) : data_(data) {}

    void range(std::size_t offset, std::size_t length) const {
        if (offset > data_.size() || length > data_.size() - offset)
            fail("truncated data at byte " + std::to_string(offset));
    }

    std::uint16_t u16(std::size_t offset) const {
        range(offset, 2);
        return static_cast<std::uint16_t>(data_[offset]) |
               (static_cast<std::uint16_t>(data_[offset + 1]) << 8);
    }

    std::uint32_t u32(std::size_t offset) const {
        range(offset, 4);
        return static_cast<std::uint32_t>(data_[offset]) |
               (static_cast<std::uint32_t>(data_[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(data_[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(data_[offset + 3]) << 24);
    }

    float f32(std::size_t offset) const {
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
                      "BSP loading requires IEEE-754 binary32");
        const auto bits = u32(offset);
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        if (!std::isfinite(result)) fail("non-finite vertex component");
        return result;
    }

    std::uint8_t u8(std::size_t offset) const {
        range(offset, 1);
        return data_[offset];
    }

private:
    const std::vector<std::uint8_t>& data_;
};

struct Lump {
    std::size_t offset;
    std::size_t length;
};
using Directory = std::unordered_map<std::uint32_t, Lump>;

Directory readDirectory(const Reader& reader, std::size_t fileSize) {
    reader.range(0, 12);
    if (reader.u32(0) != 0x50534249)
        fail("expected loose IBSP map. Fastfiles (.ff) and IWD archives are unsupported.");
    if (reader.u32(4) != 22)
        fail("unsupported version " + std::to_string(reader.u32(4)) + "; expected COD4 version 22.");
    const auto count = reader.u32(8);
    if (count == 0 || count > kMaxChunks) fail("invalid or excessive lump count");
    std::size_t offset = 12 + static_cast<std::size_t>(count) * 8;
    reader.range(0, offset);
    Directory lumps;
    for (std::size_t i = 0; i < count; ++i) {
        const auto type = reader.u32(12 + i * 8);
        const auto length = static_cast<std::size_t>(reader.u32(16 + i * 8));
        reader.range(offset, length);
        if (!lumps.emplace(type, Lump{offset, length}).second)
            fail("duplicate lump type " + std::to_string(type));
        const std::size_t end = offset + length; // range() established no overflow.
        const std::size_t padding = (4 - (length & 3)) & 3;
        // Padding is required before another lump. A last lump may end at EOF.
        if (i + 1 < count) reader.range(end, padding);
        offset = end + std::min(padding, fileSize - end);
    }
    return lumps;
}

Lump getLump(const Directory& directory, std::uint32_t type) {
    const auto found = directory.find(type);
    return found == directory.end() ? Lump{0, 0} : found->second;
}

std::size_t recordCount(Lump lump, std::size_t stride,
                        std::size_t maximum, const char* label) {
    if (lump.length == 0) fail(std::string("missing ") + label + " lump");
    if (lump.length % stride) fail(std::string(label) + " lump has a partial record");
    const auto count = lump.length / stride;
    if (count > maximum) fail(std::string(label) + " count exceeds the memory budget");
    return count;
}

bool space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

// Entity text is data, never executable script. Bound both token work and memory.
class EntityTokens {
public:
    explicit EntityTokens(std::string_view text) : text_(text) {}

    bool isBrace() const { return lastTokenWasBrace_; }

    std::optional<std::string> next() {
        lastTokenWasBrace_ = false;
        skipTrivia();
        if (cursor_ == text_.size()) return std::nullopt;
        if (++tokens_ > 1'000'000) fail("too many entity tokens");
        const char first = text_[cursor_++];
        if (first == '{' || first == '}') {
            lastTokenWasBrace_ = true;
            return std::string(1, first);
        }
        std::string result;
        const bool quoted = first == '"';
        if (!quoted) result.push_back(first);
        while (cursor_ < text_.size()) {
            const char c = text_[cursor_];
            if (quoted && c == '"') { ++cursor_; return result; }
            if (!quoted && (space(c) || c == '{' || c == '}' || c == '\0')) return result;
            if (c == '\0') fail("NUL inside an entity token");
            ++cursor_;
            if (quoted && c == '\\' && cursor_ < text_.size() &&
                (text_[cursor_] == '"' || text_[cursor_] == '\\')) {
                result.push_back(text_[cursor_++]);
            } else {
                result.push_back(c);
            }
            if (result.size() > 4096) fail("entity token exceeds 4096 bytes");
        }
        if (quoted) fail("unterminated quoted entity token");
        return result;
    }

private:
    void skipTrivia() {
        for (;;) {
            while (cursor_ < text_.size() && space(text_[cursor_])) ++cursor_;
            if (cursor_ == text_.size()) return;
            if (text_[cursor_] == '\0') {
                for (; cursor_ < text_.size(); ++cursor_)
                    if (text_[cursor_] != '\0' && !space(text_[cursor_]))
                        fail("unexpected entity data after NUL terminator");
                return;
            }
            if (cursor_ + 1 >= text_.size() || text_[cursor_] != '/') return;
            if (text_[cursor_ + 1] == '/') {
                cursor_ += 2;
                while (cursor_ < text_.size() && text_[cursor_] != '\n') ++cursor_;
            } else if (text_[cursor_ + 1] == '*') {
                const auto end = text_.find("*/", cursor_ + 2);
                if (end == std::string_view::npos) fail("unterminated entity comment");
                cursor_ = end + 2;
            } else {
                return;
            }
        }
    }

    std::string_view text_;
    std::size_t cursor_ = 0;
    std::size_t tokens_ = 0;
    bool lastTokenWasBrace_ = false;
};

template <std::size_t Count>
std::array<float, Count> parseNumbers(const std::string& text) {
    std::istringstream stream(text);
    stream.imbue(std::locale::classic());
    std::array<float, Count> values{};
    for (float& value : values)
        if (!(stream >> value) || !std::isfinite(value) || std::abs(value) > kMaxCoordinate)
            fail("invalid player spawn coordinate or angle");
    stream >> std::ws;
    if (!stream.eof()) fail("extra values in player spawn coordinate or angle");
    return values;
}

int spawnPriority(const std::string& classname) {
    if (classname == "info_player_start") return 3;
    if (classname == "info_player_deathmatch") return 2;
    if (classname.size() >= 9 && classname.compare(0, 3, "mp_") == 0 &&
        classname.compare(classname.size() - 6, 6, "_spawn") == 0) return 1;
    return 0;
}

std::optional<PlayerSpawn> readSpawn(std::string_view text) {
    EntityTokens tokenizer(text);
    std::optional<PlayerSpawn> result;
    int bestPriority = 0;
    std::size_t entities = 0;
    while (const auto opening = tokenizer.next()) {
        if (!tokenizer.isBrace() || *opening != "{") fail("expected opening entity brace");
        if (++entities > 65'536) fail("too many entities");
        std::string classname, origin, angles, angle;
        for (;;) {
            const auto key = tokenizer.next();
            if (!key) fail("unterminated entity");
            if (tokenizer.isBrace() && *key == "}") break;
            if (tokenizer.isBrace()) fail("nested entity brace");
            const auto value = tokenizer.next();
            if (!value || tokenizer.isBrace()) fail("missing entity value");
            if (*key == "classname") classname = *value;
            else if (*key == "origin") origin = *value;
            else if (*key == "angles") angles = *value;
            else if (*key == "angle") angle = *value;
        }
        const int priority = spawnPriority(classname);
        if (priority <= bestPriority || origin.empty()) continue;
        PlayerSpawn spawn;
        spawn.position = parseNumbers<3>(origin);
        if (!angles.empty()) spawn.angles = parseNumbers<3>(angles);
        else if (!angle.empty()) {
            const float yaw = parseNumbers<1>(angle)[0];
            if (yaw == -1.0f) spawn.angles[0] = -90.0f;
            else if (yaw == -2.0f) spawn.angles[0] = 90.0f;
            else spawn.angles[1] = yaw;
        }
        result = spawn;
        bestPriority = priority;
    }
    return result;
}

} // namespace

std::optional<PlayerSpawn> parseBspSpawn(std::string_view entityText) {
    if (entityText.size() > BspLoadLimits{}.maxEntityBytes)
        fail("entity text exceeds the memory budget");
    return readSpawn(entityText);
}

BspMesh loadBsp(const std::vector<std::uint8_t>& bytes, const BspLoadLimits& requested) {
    const auto limits = boundedLimits(requested);
    if (bytes.size() > limits.maxFileBytes) fail("file exceeds the memory budget (maximum 128 MiB)");
    const Reader reader(bytes);
    const Directory directory = readDirectory(reader, bytes.size());
    BspMesh mesh;
    mesh.layered = getLump(directory, kSimpleTriangles).length == 0;
    const auto surfaces = getLump(directory, mesh.layered ? kTriangles : kSimpleTriangles);
    const auto vertices = getLump(directory, mesh.layered ? kVertices : kSimpleVertices);
    const auto indices = getLump(directory, mesh.layered ? kIndices : kSimpleIndices);
    const auto surfaceCount = recordCount(surfaces, kSurfaceStride, limits.maxSurfaces, "surface");
    const auto vertexCount = recordCount(vertices, kVertexStride, limits.maxVertices, "vertex");
    const auto diskIndexCount = recordCount(indices, 2, limits.maxIndices, "index");
    const auto entities = getLump(directory, kEntities);
    if (entities.length > limits.maxEntityBytes) fail("entity text exceeds the memory budget");

    // Validate every surface before reserving output memory. Index ranges may
    // overlap on disk, so budget the expanded output sum as well as the lump.
    std::size_t outputIndexCount = 0;
    for (std::size_t i = 0; i < surfaceCount; ++i) {
        const auto offset = surfaces.offset + i * kSurfaceStride;
        const auto firstVertex = reader.u32(offset + 12);
        const auto count = reader.u16(offset + 16);
        const auto indexCount = reader.u16(offset + 18);
        const auto firstIndex = reader.u32(offset + 20);
        if (count == 0 || firstVertex > vertexCount || count > vertexCount - firstVertex)
            fail("surface " + std::to_string(i) + " vertex range is out of bounds");
        if (indexCount == 0 || indexCount % 3 != 0)
            fail("surface " + std::to_string(i) + " has no complete triangle list");
        if (firstIndex > 0x7fffffffU || firstIndex > diskIndexCount ||
            indexCount > diskIndexCount - firstIndex)
            fail("surface " + std::to_string(i) + " index range is out of bounds");
        if (indexCount > limits.maxIndices - outputIndexCount)
            fail("expanded index count exceeds the memory budget");
        outputIndexCount += indexCount;
    }

    mesh.vertices.reserve(vertexCount);
    for (std::size_t i = 0; i < vertexCount; ++i) {
        const auto offset = vertices.offset + i * kVertexStride;
        BspVertex vertex;
        double lengthSquared = 0;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            vertex.position[axis] = reader.f32(offset + axis * 4);
            if (std::abs(vertex.position[axis]) > kMaxCoordinate)
                fail("vertex position exceeds the coordinate budget");
            vertex.normal[axis] = reader.f32(offset + 12 + axis * 4);
            lengthSquared += static_cast<double>(vertex.normal[axis]) * vertex.normal[axis];
        }
        if (lengthSquared > 1e-20) {
            const double inverseLength = 1.0 / std::sqrt(lengthSquared);
            for (float& component : vertex.normal) component = static_cast<float>(component * inverseLength);
        } else {
            vertex.normal = {0.0f, 0.0f, 1.0f};
        }
        vertex.color = {reader.u8(offset + 26) / 255.0f, reader.u8(offset + 25) / 255.0f,
                        reader.u8(offset + 24) / 255.0f, reader.u8(offset + 27) / 255.0f};
        mesh.vertices.push_back(vertex);
    }

    mesh.indices.reserve(outputIndexCount);
    mesh.minBounds.fill(std::numeric_limits<float>::max());
    mesh.maxBounds.fill(std::numeric_limits<float>::lowest());
    for (std::size_t i = 0; i < surfaceCount; ++i) {
        const auto offset = surfaces.offset + i * kSurfaceStride;
        const auto baseVertex = reader.u32(offset + 12);
        const auto count = reader.u16(offset + 16);
        const auto indexCount = reader.u16(offset + 18);
        const auto firstIndex = reader.u32(offset + 20);
        for (std::size_t j = 0; j < indexCount; ++j) {
            const auto relative = reader.u16(indices.offset + (firstIndex + j) * 2);
            if (relative >= count)
                fail("surface " + std::to_string(i) + " index exceeds its local vertex range");
            const std::uint32_t absolute = baseVertex + relative;
            mesh.indices.push_back(absolute);
            const auto& position = mesh.vertices[absolute].position;
            for (std::size_t axis = 0; axis < 3; ++axis) {
                mesh.minBounds[axis] = std::min(mesh.minBounds[axis], position[axis]);
                mesh.maxBounds[axis] = std::max(mesh.maxBounds[axis], position[axis]);
            }
        }
    }
    mesh.surfaceCount = static_cast<std::uint32_t>(surfaceCount);
    if (entities.length != 0)
        mesh.spawn = readSpawn(std::string_view(
            reinterpret_cast<const char*>(bytes.data() + entities.offset), entities.length));
    return mesh;
}

BspMesh loadBsp(const std::string& path, const BspLoadLimits& requested) {
    const auto limits = boundedLimits(requested);
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw BspError("Cannot open the map file: " + path);
    const auto end = input.tellg();
    if (end < 0) fail("cannot determine map file size");
    const auto size = static_cast<std::uintmax_t>(end);
    if (size > limits.maxFileBytes) fail("file exceeds the memory budget (maximum 128 MiB)");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    if (!bytes.empty() && !input.read(reinterpret_cast<char*>(bytes.data()),
                                      static_cast<std::streamsize>(bytes.size())))
        fail("map file changed or was truncated while reading");
    // Catch growth as well: a concurrently replaced/modified file is not trusted.
    if (input.peek() != std::char_traits<char>::eof()) fail("map file changed while reading");
    return loadBsp(bytes, limits);
}

} // namespace kisakcod::assets
