#pragma once

#include "data/GameData.hpp"
#include "world/Chunk.hpp"

namespace voxel {
struct TerrainGenerator {
    int seed = 12345;
    Chunk generateChunk(const ChunkCoord& coord, const GameData& gameData) const;
};
}  // namespace voxel