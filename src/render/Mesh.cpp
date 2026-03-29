#include "render/Mesh.hpp"

#include <array>
#include <cstddef>
#include <vector>

#include <GLFW/glfw3.h>

namespace voxel {
namespace {
struct MaskCell {
    std::uint8_t block = 0;
};

MeshSurface& surfaceForTexture(ChunkMesh& mesh, const BlockDefinition& block) {
    const std::string albedo = block.textures.albedo.value_or("");
    const std::string emissive = block.textures.emissive.value_or("");
    for (auto& surface : mesh.surfaces) {
        if (surface.albedoTexturePath == albedo &&
            surface.emissiveTexturePath == emissive &&
            surface.translucent == block.translucent &&
            surface.opacity == block.opacity) {
            return surface;
        }
    }

    mesh.surfaces.push_back({albedo, emissive, block.translucent, block.opacity, {}});
    return mesh.surfaces.back();
}

bool isFaceVisible(
    const GameData& gameData,
    const std::uint8_t currentBlock,
    const std::uint8_t neighborBlock
) {
    if (neighborBlock == 0) {
        return true;
    }
    if (neighborBlock == currentBlock) {
        return false;
    }

    const BlockDefinition* current = findBlockDefinitionForBlockType(gameData, currentBlock);
    const BlockDefinition* neighbor = findBlockDefinitionForBlockType(gameData, neighborBlock);
    if (current == nullptr) {
        return false;
    }
    if (neighbor == nullptr) {
        return true;
    }
    if (current->translucent) {
        return neighbor->id != current->id;
    }
    return !neighbor->solid || neighbor->translucent;
}

void appendTriangle(
    MeshSurface& surface,
    const Vec3& a,
    const Vec3& b,
    const Vec3& c,
    const Vec3& normal,
    const Color& color,
    const std::array<std::array<float, 2>, 3>& uv
) {
    surface.vertices.push_back({a, normal, color, uv[0][0], uv[0][1]});
    surface.vertices.push_back({b, normal, color, uv[1][0], uv[1][1]});
    surface.vertices.push_back({c, normal, color, uv[2][0], uv[2][1]});
}

float faceBrightness(const int axis, const int direction) {
    if (axis == 1) {
        return direction > 0 ? 1.00f : 0.45f;
    }
    if (axis == 2) {
        return direction > 0 ? 0.78f : 0.60f;
    }
    return direction > 0 ? 0.72f : 0.52f;
}

void appendQuad(
    ChunkMesh& mesh,
    const std::array<Vec3, 4>& quad,
    const Vec3& normal,
    const BlockDefinition& block,
    const float brightness,
    const float width,
    const float height
) {
    MeshSurface& surface = surfaceForTexture(mesh, block);
    const Color shaded {brightness, brightness, brightness};

    appendTriangle(surface, quad[0], quad[1], quad[2], normal, shaded, {{{0.0f, 0.0f}, {width, 0.0f}, {width, height}}});
    appendTriangle(surface, quad[0], quad[2], quad[3], normal, shaded, {{{0.0f, 0.0f}, {width, height}, {0.0f, height}}});
    mesh.visibleFaces += 1;
    mesh.triangleCount += 2;
}

void appendGreedyQuad(
    ChunkMesh& mesh,
    const ChunkCoord& coord,
    const int axis,
    const int direction,
    const int slice,
    const int startU,
    const int startV,
    const int width,
    const int height,
    const BlockDefinition& block
) {
    const int u = (axis + 1) % 3;
    const int v = (axis + 2) % 3;

    std::array<float, 3> base {
        static_cast<float>(coord.x * kChunkX),
        static_cast<float>(coord.y * kChunkY),
        static_cast<float>(coord.z * kChunkZ)
    };
    base[axis] += static_cast<float>(direction > 0 ? slice + 1 : slice);
    base[u] += static_cast<float>(startU);
    base[v] += static_cast<float>(startV);

    std::array<float, 3> du {0.0f, 0.0f, 0.0f};
    du[u] = static_cast<float>(width);

    std::array<float, 3> dv {0.0f, 0.0f, 0.0f};
    dv[v] = static_cast<float>(height);

    const Vec3 p0 {base[0], base[1], base[2]};
    const Vec3 p1 {base[0] + du[0], base[1] + du[1], base[2] + du[2]};
    const Vec3 p2 {base[0] + du[0] + dv[0], base[1] + du[1] + dv[1], base[2] + du[2] + dv[2]};
    const Vec3 p3 {base[0] + dv[0], base[1] + dv[1], base[2] + dv[2]};

    Vec3 normal {0.0f, 0.0f, 0.0f};
    if (axis == 0) {
        normal.x = static_cast<float>(direction);
    } else if (axis == 1) {
        normal.y = static_cast<float>(direction);
    } else {
        normal.z = static_cast<float>(direction);
    }

    if (direction > 0) {
        appendQuad(mesh, {p0, p1, p2, p3}, normal, block, faceBrightness(axis, direction), static_cast<float>(width), static_cast<float>(height));
    } else {
        appendQuad(mesh, {p0, p3, p2, p1}, normal, block, faceBrightness(axis, direction), static_cast<float>(height), static_cast<float>(width));
    }
}
}  // namespace

ChunkMesh buildChunkMesh(const World& world, const ChunkCoord& coord, const GameData& gameData) {
    ChunkMesh mesh;
    mesh.coord = coord;

    if (!chunkLoaded(world, coord)) {
        return mesh;
    }
    const ChunkBlocks& blocks = getChunk(world, coord).blocks;
    const int worldMinX = coord.x * kChunkX;
    const int worldMinY = coord.y * kChunkY;
    const int worldMinZ = coord.z * kChunkZ;

    for (int x = 0; x < kChunkX; ++x) {
        for (int y = 0; y < kChunkY; ++y) {
            for (int z = 0; z < kChunkZ; ++z) {
                if (blocks[x][y][z] != 0) {
                    mesh.solidBlocks += 1;
                }
            }
        }
    }

    if (mesh.solidBlocks == 0) {
        return mesh;
    }

    constexpr std::array<int, 3> dims {kChunkX, kChunkY, kChunkZ};

    for (int axis = 0; axis < 3; ++axis) {
        const int u = (axis + 1) % 3;
        const int v = (axis + 2) % 3;
        std::vector<MaskCell> mask(static_cast<std::size_t>(dims[u] * dims[v]));

        for (int direction : {1, -1}) {
            for (int slice = 0; slice < dims[axis]; ++slice) {
                // Only border slices need world (map) lookups for the neighbor
                const bool neighborOutside = (direction == 1 && slice == dims[axis] - 1) ||
                                             (direction == -1 && slice == 0);

                for (int row = 0; row < dims[v]; ++row) {
                    for (int col = 0; col < dims[u]; ++col) {
                        std::array<int, 3> local {0, 0, 0};
                        local[axis] = slice;
                        local[u] = col;
                        local[v] = row;

                        const std::uint8_t block = blocks[local[0]][local[1]][local[2]];
                        MaskCell& entry = mask[static_cast<std::size_t>(row * dims[u] + col)];
                        entry.block = 0;

                        if (block == 0) {
                            continue;
                        }

                        std::uint8_t neighbor;
                        if (neighborOutside) {
                            const int nx = worldMinX + local[0] + (axis == 0 ? direction : 0);
                            const int ny = worldMinY + local[1] + (axis == 1 ? direction : 0);
                            const int nz = worldMinZ + local[2] + (axis == 2 ? direction : 0);
                            neighbor = getBlock(world, nx, ny, nz);
                        } else {
                            std::array<int, 3> nl = local;
                            nl[axis] += direction;
                            neighbor = blocks[nl[0]][nl[1]][nl[2]];
                        }

                        if (isFaceVisible(gameData, block, neighbor)) {
                            entry.block = block;
                        }
                    }
                }

                for (int row = 0; row < dims[v]; ++row) {
                    for (int col = 0; col < dims[u];) {
                        const MaskCell cell = mask[static_cast<std::size_t>(row * dims[u] + col)];
                        if (cell.block == 0) {
                            ++col;
                            continue;
                        }

                        int width = 1;
                        while (col + width < dims[u] &&
                               mask[static_cast<std::size_t>(row * dims[u] + col + width)].block == cell.block) {
                            ++width;
                        }

                        int height = 1;
                        bool canGrow = true;
                        while (row + height < dims[v] && canGrow) {
                            for (int k = 0; k < width; ++k) {
                                if (mask[static_cast<std::size_t>((row + height) * dims[u] + col + k)].block != cell.block) {
                                    canGrow = false;
                                    break;
                                }
                            }
                            if (canGrow) {
                                ++height;
                            }
                        }

                        const BlockDefinition* blockDefinition = findBlockDefinitionForBlockType(gameData, cell.block);
                        if (blockDefinition != nullptr) {
                            appendGreedyQuad(mesh, coord, axis, direction, slice, col, row, width, height, *blockDefinition);
                        }

                        for (int dy = 0; dy < height; ++dy) {
                            for (int dx = 0; dx < width; ++dx) {
                                mask[static_cast<std::size_t>((row + dy) * dims[u] + col + dx)].block = 0;
                            }
                        }

                        col += width;
                    }
                }
            }
        }
    }

    return mesh;
}

void uploadChunkMesh(ChunkMesh& mesh) {
    for (auto& surface : mesh.surfaces) {
        if (surface.vertices.empty()) {
            continue;
        }
        surface.vertexCount = static_cast<int>(surface.vertices.size());
        glGenBuffers(1, &surface.vboId);
        glBindBuffer(GL_ARRAY_BUFFER, surface.vboId);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(surface.vertices.size() * sizeof(MeshVertex)),
            surface.vertices.data(),
            GL_STATIC_DRAW
        );
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        surface.vertices.clear();
        surface.vertices.shrink_to_fit();
    }
}

void destroyChunkMesh(ChunkMesh& mesh) {
    for (auto& surface : mesh.surfaces) {
        if (surface.vboId != 0) {
            glDeleteBuffers(1, &surface.vboId);
            surface.vboId = 0;
            surface.vertexCount = 0;
        }
    }
}
}  // namespace voxel
