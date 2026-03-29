#include "world/TerrainGenerator.hpp"

#include "FastNoiseLite.h"

namespace voxel {
Chunk TerrainGenerator::generateChunk(const ChunkCoord& coord, const GameData& gameData) const {
    Chunk chunk;
    chunk.coord = coord;

    const std::uint8_t grassId = runtimeIdForBlock(gameData, "grass");
    const std::uint8_t dirtId  = runtimeIdForBlock(gameData, "dirt");
    const std::uint8_t stoneId = runtimeIdForBlock(gameData, "stone");

    FastNoiseLite noise;
    noise.SetSeed(seed);
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetFractalOctaves(4);
    noise.SetFractalLacunarity(2.0f);
    noise.SetFractalGain(0.5f);
    noise.SetFrequency(0.003f);

    const int baseX = coord.x * kChunkX;
    const int baseY = coord.y * kChunkY;
    const int baseZ = coord.z * kChunkZ;

    for (int lx = 0; lx < kChunkX; ++lx) {
        for (int lz = 0; lz < kChunkZ; ++lz) {
            const int wx = baseX + lx;
            const int wz = baseZ + lz;

            // n is roughly in [-1, 1]; map to a surface height in [12, 52]
            const float n = noise.GetNoise(static_cast<float>(wx), static_cast<float>(wz));
            const int surfaceY = 32 + static_cast<int>(n * 20.0f);

            for (int ly = 0; ly < kChunkY; ++ly) {
                const int wy = baseY + ly;

                std::uint8_t blockType = 0;
                if (wy == surfaceY) {
                    blockType = grassId;
                } else if (wy < surfaceY && wy >= surfaceY - 3) {
                    blockType = dirtId;
                } else if (wy < surfaceY - 3) {
                    blockType = stoneId;
                }
                chunk.blocks[lx][ly][lz] = blockType;
            }
        }
    }

    return chunk;
}
}  // namespace voxel
