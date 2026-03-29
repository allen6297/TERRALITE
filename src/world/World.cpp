#include "world/World.hpp"

#include <algorithm>

namespace voxel {
namespace {
int floorDiv(const int a, const int b) {
    return a / b - (a % b != 0 && (a ^ b) < 0 ? 1 : 0);
}

int floorMod(const int a, const int b) {
    return ((a % b) + b) % b;
}
}  // namespace

bool isYInBounds(const int y) {
    return y >= 0 && y < kWorldY;
}

bool inChunkBounds(const ChunkCoord& coord) {
    return coord.y >= 0 && coord.y < kChunkCountY;
}

ChunkCoord worldToChunkCoord(const int x, const int y, const int z) {
    return {floorDiv(x, kChunkX), floorDiv(y, kChunkY), floorDiv(z, kChunkZ)};
}

Int3 worldToLocalBlock(const int x, const int y, const int z) {
    return {floorMod(x, kChunkX), floorMod(y, kChunkY), floorMod(z, kChunkZ)};
}

bool chunkLoaded(const World& world, const ChunkCoord& coord) {
    return world.chunks.count(coord) > 0;
}

Chunk& getChunk(World& world, const ChunkCoord& coord) {
    return world.chunks.at(coord);
}

const Chunk& getChunk(const World& world, const ChunkCoord& coord) {
    return world.chunks.at(coord);
}

std::uint8_t getBlock(const World& world, const int x, const int y, const int z) {
    if (!isYInBounds(y)) {
        return 0;
    }
    const ChunkCoord coord = worldToChunkCoord(x, y, z);
    const auto it = world.chunks.find(coord);
    if (it == world.chunks.end()) {
        return 0;
    }
    const Int3 local = worldToLocalBlock(x, y, z);
    return it->second.blocks[local.x][local.y][local.z];
}

void setBlock(World& world, const int x, const int y, const int z, const std::uint8_t block) {
    if (!isYInBounds(y)) {
        return;
    }
    const ChunkCoord coord = worldToChunkCoord(x, y, z);
    const auto it = world.chunks.find(coord);
    if (it == world.chunks.end()) {
        return;
    }
    const Int3 local = worldToLocalBlock(x, y, z);
    it->second.blocks[local.x][local.y][local.z] = block;
}

bool isSolid(const World& world, const GameData& gameData, const int x, const int y, const int z) {
    const std::uint8_t block = getBlock(world, x, y, z);
    return block != 0 && gameData.solidByRuntimeId[block];
}

bool isOccupied(const World& world, const int x, const int y, const int z) {
    return getBlock(world, x, y, z) != 0;
}

std::optional<RaycastHit> raycastWorld(const World& world, const GameData& gameData, const Vec3& origin, const Vec3& direction) {
    Vec3 previous = origin;

    for (float distance = 0.0f; distance <= kReach; distance += kRayStep) {
        const Vec3 point = origin + (direction * distance);
        const Int3 block {
            static_cast<int>(std::floor(point.x)),
            static_cast<int>(std::floor(point.y)),
            static_cast<int>(std::floor(point.z))
        };

        if (!isYInBounds(block.y)) {
            previous = point;
            continue;
        }

        if (isSolid(world, gameData, block.x, block.y, block.z)) {
            return RaycastHit {
                block,
                {
                    static_cast<int>(std::floor(previous.x)),
                    static_cast<int>(std::floor(previous.y)),
                    static_cast<int>(std::floor(previous.z))
                },
                getBlock(world, block.x, block.y, block.z)
            };
        }

        previous = point;
    }

    return std::nullopt;
}
}  // namespace voxel
