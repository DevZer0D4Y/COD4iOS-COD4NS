#pragma once
#include "BspLoader.hpp"

namespace kisakcod::assets {
// Geometry-only reader for PC COD4 IWffu100/version 5. It locates a validated
// GfxWorld disk32 record using its inline baseName, then walks its serialized
// subobjects in the order documented by upstream Load_GfxWorld. This is NOT
// the engine's asset database: external asset references are never dereferenced.
// The original map basename must be preserved (e.g. mp_shipment.ff).
BspMesh loadFastfileWorld(const std::vector<std::uint8_t>& compressed,
                         const std::string& mapBaseName);
BspMesh loadMap(const std::string& path); // Detects IBSP vs fastfile by magic.
}
