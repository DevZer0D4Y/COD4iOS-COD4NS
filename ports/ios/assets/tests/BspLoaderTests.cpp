#include "BspLoader.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <utility>

using namespace kisakcod::assets;

namespace {
using Bytes = std::vector<std::uint8_t>;
using Chunk = std::pair<std::uint32_t, Bytes>;
int assertions = 0;

void check(bool ok, const std::string& description) {
    ++assertions;
    if (!ok) throw std::runtime_error("Failed: " + description);
}

void fails(const std::function<void()>& operation, const std::string& fragment) {
    try { operation(); }
    catch (const BspError& error) {
        check(std::string(error.what()).find(fragment) != std::string::npos,
              "error must mention '" + fragment + "', got '" + error.what() + "'");
        return;
    }
    throw std::runtime_error("Expected BspError containing: " + fragment);
}

void u16(Bytes& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

void u32(Bytes& bytes, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
}

void set32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (i * 8));
}

void f32(Bytes& bytes, float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, 4);
    u32(bytes, bits);
}

void vertex(Bytes& bytes, float x, float y, float z) {
    for (const float component : {x, y, z, 0.0f, 0.0f, 2.0f}) f32(bytes, component);
    for (const auto component : {32, 64, 128, 255}) bytes.push_back(component); // BGRA.
    for (int i = 0; i < 10; ++i) f32(bytes, 0.0f);
}

void surface(Bytes& bytes, std::uint32_t baseVertex, std::uint16_t vertexCount,
             std::uint16_t indexCount, std::uint32_t firstIndex) {
    u16(bytes, 0); // materialIndex
    for (int i = 0; i < 6; ++i) bytes.push_back(0); // light/probe/flags/padding
    u32(bytes, 0); // vertex layer data
    u32(bytes, baseVertex);
    u16(bytes, vertexCount);
    u16(bytes, indexCount);
    u32(bytes, firstIndex);
}

Bytes bsp(const std::vector<Chunk>& chunks) {
    Bytes bytes{'I', 'B', 'S', 'P'};
    u32(bytes, 22);
    u32(bytes, static_cast<std::uint32_t>(chunks.size()));
    for (const auto& chunk : chunks) {
        u32(bytes, chunk.first);
        u32(bytes, static_cast<std::uint32_t>(chunk.second.size()));
    }
    for (const auto& chunk : chunks) {
        bytes.insert(bytes.end(), chunk.second.begin(), chunk.second.end());
        while (bytes.size() % 4) bytes.push_back(0);
    }
    return bytes;
}

Bytes textBytes(const std::string& value) { return Bytes(value.begin(), value.end()); }

std::vector<Chunk> mapChunks(bool simple = false) {
    Bytes vertices, surfaces, indices;
    vertex(vertices, -999.0f, -999.0f, -999.0f); // Unreferenced: must not skew camera bounds.
    vertex(vertices, 0.0f, 0.0f, 0.0f);
    vertex(vertices, 10.0f, 0.0f, 0.0f);
    vertex(vertices, 0.0f, 10.0f, 0.0f);
    vertex(vertices, -10.0f, 0.0f, 10.0f);
    vertex(vertices, 0.0f, 0.0f, 10.0f);
    vertex(vertices, 0.0f, 10.0f, 10.0f);
    surface(surfaces, 1, 3, 3, 2);
    surface(surfaces, 4, 3, 3, 5);
    for (const auto index : {65535, 65535, 0, 1, 2, 2, 0, 1}) u16(indices, index);
    // Deliberately out-of-order directory, and an odd unknown lump before data.
    return {{99, {1, 2, 3}}, {simple ? 0x31u : 0x0bu, indices},
            {simple ? 0x2fu : 0x09u, surfaces}, {simple ? 0x30u : 0x0au, vertices}};
}

void testGeometryAndEntities() {
    auto chunks = mapChunks();
    chunks.push_back({0x27, textBytes(
        "// Spawn order should not win over SP spawn priority\n"
        "{\"classname\" \"info_player_deathmatch\" \"origin\" \"1 2 3\"}\n"
        "{\"classname\" \"worldspawn\" \"origin\" \"unparsed\"}\n"
        "/* comment */ {\"classname\" \"info_player_start\" \"origin\" \"4 5 6\""
        " \"angles\" \"10 90 0\"}\n")});
    const auto mesh = loadBsp(bsp(chunks));
    check(mesh.vertices.size() == 7, "all disk vertices loaded");
    check(mesh.indices == std::vector<std::uint32_t>({1, 2, 3, 6, 4, 5}),
          "nonzero surface firstIndex and firstVertex combine exactly once");
    check(mesh.surfaceCount == 2 && mesh.layered, "layered metadata");
    check(mesh.minBounds == std::array<float, 3>{-10, 0, 0}, "bounds ignore unused vertices");
    check(mesh.maxBounds == std::array<float, 3>{10, 10, 10}, "max bounds");
    check(mesh.vertices[1].normal == std::array<float, 3>{0, 0, 1}, "normal normalization");
    check(std::abs(mesh.vertices[1].color[0] - 128.0f / 255.0f) < 1e-6f, "BGRA red decoded");
    check(std::abs(mesh.vertices[1].color[2] - 32.0f / 255.0f) < 1e-6f, "BGRA blue decoded");
    check(mesh.vertices[1].color[3] == 1, "alpha decoded");
    check(mesh.spawn && mesh.spawn->position == std::array<float, 3>{4, 5, 6}, "preferred spawn");
    check(mesh.spawn->angles == std::array<float, 3>{10, 90, 0}, "spawn angles");

    auto simple = mapChunks(true);
    check(!loadBsp(bsp(simple)).layered, "unlayered-only maps supported");
    const auto layered = mapChunks();
    simple.insert(simple.end(), layered.begin() + 1, layered.end());
    const auto combined = loadBsp(bsp(simple));
    check(!combined.layered && combined.indices.size() == 6, "two representations never overlaid");
    auto emptySimple = mapChunks();
    emptySimple.push_back({0x2f, {}});
    check(loadBsp(bsp(emptySimple)).layered, "empty simple lump falls back to layered");

    chunks = mapChunks();
    chunks.push_back({0x27, textBytes("{classname mp_tdm_spawn origin \"7 8 9\" angle 180}")});
    check(loadBsp(bsp(chunks)).spawn->angles[1] == 180, "MP spawn and scalar yaw");
    chunks.back().second = textBytes("{classname info_player_start origin \"0 0 0\" angle -1}");
    check(loadBsp(bsp(chunks)).spawn->angles[0] == -90, "upward special angle");
    chunks.back().second.push_back(0);
    check(loadBsp(bsp(chunks)).spawn.has_value(), "terminal entity NUL is accepted");
    chunks.back().second = textBytes("{classname worldspawn literal \"}\"}");
    check(!loadBsp(bsp(chunks)).spawn, "quoted braces are entity values, not structural syntax");

    chunks = mapChunks();
    chunks.push_back({100, {1}});
    auto withoutFinalPadding = bsp(chunks);
    withoutFinalPadding.resize(withoutFinalPadding.size() - 3);
    check(loadBsp(withoutFinalPadding).indices.size() == 6, "last lump can end without padding");
    chunks = mapChunks();
    set32(chunks[3].second, 20, 0);
    check(loadBsp(bsp(chunks)).vertices[0].normal[2] == 1, "zero normal has finite fallback");
}

void testMalformed() {
    const auto good = bsp(mapChunks());
    for (std::size_t length = 0; length < good.size(); ++length) {
        Bytes truncated(good.begin(), good.begin() + length);
        fails([&] { loadBsp(truncated); }, "truncated");
    }
    auto bad = good;
    bad[0] = 'X';
    fails([&] { loadBsp(bad); }, "expected loose IBSP");
    set32(bad = good, 4, 21);
    fails([&] { loadBsp(bad); }, "expected COD4 version 22");
    set32(bad = good, 8, 0xffffffff);
    fails([&] { loadBsp(bad); }, "lump count");
    set32(bad = good, 16, 0xffffffff);
    fails([&] { loadBsp(bad); }, "truncated");
    auto chunks = mapChunks();
    chunks.push_back(chunks[0]);
    fails([&] { loadBsp(bsp(chunks)); }, "duplicate lump");
    chunks = mapChunks();
    chunks.pop_back();
    fails([&] { loadBsp(bsp(chunks)); }, "missing vertex");
    chunks = mapChunks();
    chunks[3].second.pop_back();
    fails([&] { loadBsp(bsp(chunks)); }, "partial record");
    chunks = mapChunks();
    set32(chunks[2].second, 12, 0xffffffff);
    fails([&] { loadBsp(bsp(chunks)); }, "vertex range");
    chunks = mapChunks();
    set32(chunks[2].second, 20, 0xffffffff);
    fails([&] { loadBsp(bsp(chunks)); }, "index range");
    chunks = mapChunks();
    chunks[2].second[18] = 4;
    fails([&] { loadBsp(bsp(chunks)); }, "complete triangle");
    chunks = mapChunks();
    chunks[1].second[4] = 3; // Inside global vertices but outside surface-local range.
    fails([&] { loadBsp(bsp(chunks)); }, "local vertex range");
    chunks = mapChunks();
    set32(chunks[3].second, 0, 0x7fc00000);
    fails([&] { loadBsp(bsp(chunks)); }, "non-finite");
    chunks = mapChunks();
    set32(chunks[3].second, 12, 0x7f800000);
    fails([&] { loadBsp(bsp(chunks)); }, "non-finite");
    chunks = mapChunks();
    set32(chunks[3].second, 0, 0x7f7fffff);
    fails([&] { loadBsp(bsp(chunks)); }, "coordinate budget");
    chunks = mapChunks();
    chunks.push_back({0x2f, mapChunks(true)[2].second});
    fails([&] { loadBsp(bsp(chunks)); }, "missing vertex");

    for (const std::string entity : {"{\"classname\"", "{{}}", "{key}", "/* unclosed",
                                    "{classname info_player_start origin \"nan 0 0\"}",
                                    "{classname info_player_start origin \"1 2 3 extra\"}"}) {
        chunks = mapChunks();
        chunks.push_back({0x27, textBytes(entity)});
        fails([&] { loadBsp(bsp(chunks)); }, "IBSP:");
    }
}

void testBudgetsAndFile() {
    auto chunks = mapChunks();
    const auto good = bsp(chunks);
    BspLoadLimits limits;
    limits.maxFileBytes = good.size() - 1;
    fails([&] { loadBsp(good, limits); }, "file exceeds");
    limits = {};
    limits.maxVertices = 6;
    fails([&] { loadBsp(good, limits); }, "vertex count");
    limits = {};
    limits.maxSurfaces = 1;
    fails([&] { loadBsp(good, limits); }, "surface count");
    limits = {};
    limits.maxIndices = 7;
    fails([&] { loadBsp(good, limits); }, "index count");
    limits = {};
    chunks[2].second.clear();
    for (int i = 0; i < 3; ++i) surface(chunks[2].second, 1, 3, 3, 2);
    limits.maxIndices = 8;
    fails([&] { loadBsp(bsp(chunks), limits); }, "expanded index count");
    limits = {};
    limits.maxEntityBytes = 1;
    chunks = mapChunks();
    chunks.push_back({0x27, textBytes("{}")});
    fails([&] { loadBsp(bsp(chunks), limits); }, "entity text");

    // File roundtrip uses an isolated temporary directory and removes it again.
    const auto path = std::filesystem::temp_directory_path() /
        ("kisakcod-bsp-test-" + std::to_string(std::random_device{}()));
    check(std::filesystem::create_directory(path), "create test directory");
    struct RemoveDirectory {
        std::filesystem::path path;
        ~RemoveDirectory() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
    } cleanup{path};
    const auto filename = (path / "synthetic-format-test.bsp").string();
    { std::ofstream output(filename, std::ios::binary);
      output.write(reinterpret_cast<const char*>(good.data()), good.size()); }
    check(loadBsp(filename).indices.size() == 6, "path overload roundtrip");
    limits = {};
    limits.maxFileBytes = 1;
    fails([&] { loadBsp(filename, limits); }, "file exceeds");
    fails([&] { loadBsp((path / "missing.bsp").string()); }, "Cannot open");
}

void testDeterministicMutations() {
    const auto good = bsp(mapChunks());
    std::mt19937 random(0x434f4434);
    BspLoadLimits limits;
    limits.maxFileBytes = 4096;
    limits.maxIndices = 100;
    limits.maxSurfaces = 100;
    limits.maxVertices = 100;
    // Memory-safety exercise under ASan/UBSan: corrupt data must either reject or
    // return an internally consistent mesh. Some ignored metadata edits are valid.
    for (int trial = 0; trial < 2000; ++trial) {
        auto bytes = good;
        for (int mutation = 0; mutation < 1 + trial % 4; ++mutation)
            bytes[random() % bytes.size()] = static_cast<std::uint8_t>(random());
        try {
            const auto mesh = loadBsp(bytes, limits);
            for (auto index : mesh.indices) check(index < mesh.vertices.size(), "mutated index valid");
            check(mesh.indices.size() % 3 == 0, "mutated triangle list valid");
        } catch (const BspError&) { ++assertions; }
    }
}
} // namespace

int main() {
    try {
        testGeometryAndEntities();
        testMalformed();
        testBudgetsAndFile();
        testDeterministicMutations();
        std::cout << "BSP parser checks passed: " << assertions << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
