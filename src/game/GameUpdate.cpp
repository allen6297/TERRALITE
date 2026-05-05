#include "game/Game.hpp"

#include <algorithm>
#include <chrono>
#include <unordered_set>

#include "GameInternal.hpp"

namespace voxel {

void Game::update(GLFWwindow* window, const float deltaTime) {
    gameTimeSeconds_ += deltaTime;
    frameTimeMs_ = deltaTime * 1000.0f;
    fpsAccumulator_ += deltaTime;
    ++fpsFrameCount_;
    if (fpsAccumulator_ >= 0.5f) {
        fps_ = static_cast<int>(static_cast<float>(fpsFrameCount_) / fpsAccumulator_);
        fpsAccumulator_ = 0.0f;
        fpsFrameCount_ = 0;
    }

    collectPending();

    updateMouseLook(window, player_);
    updateInput(window, input_);
    handleInventorySelection(window);
    jump(player_, input_);
    updateMovement(window, world_, gameData_, player_, deltaTime);
    simulateLiquids(deltaTime);
    processBlockTicks();

    currentHit_ = raycastWorld(world_, gameData_, getEyePosition(player_), getLookDirection(player_));
    handleBlockActions();

    const bool f5Now = glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS;
    if (f5Now && !f5WasPressed_) {
        reloadGameData();
    }
    f5WasPressed_ = f5Now;

    const int px = static_cast<int>(std::floor(player_.position.x));
    const int py = std::clamp(static_cast<int>(std::floor(player_.position.y)), 0, kWorldY - 1);
    const int pz = static_cast<int>(std::floor(player_.position.z));
    updateLoadedChunks(worldToChunkCoord(px, py, pz));
}

void Game::collectPending() {
    for (auto it = pendingTerrain_.begin(); it != pendingTerrain_.end();) {
        if (it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const ChunkCoord coord = it->first;
            world_.chunks[coord] = it->second.get();
            scheduleChunkBlockTicks(world_.chunks[coord]);
            it = pendingTerrain_.erase(it);
            launchMeshBuild(coord);
        } else {
            ++it;
        }
    }

    for (auto it = pendingMeshes_.begin(); it != pendingMeshes_.end();) {
        if (it->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const ChunkCoord coord = it->coord;
            ChunkMesh mesh = it->future.get();
            it = pendingMeshes_.erase(it);
            const auto existing = meshes_.find(coord);
            if (existing != meshes_.end()) {
                destroyChunkMesh(existing->second);
            }
            uploadChunkMesh(mesh);
            meshes_[coord] = std::move(mesh);
        } else {
            ++it;
        }
    }
}

void Game::processBlockTicks() {
    constexpr int kMaxBlockTicksPerFrame = 64;
    int processed = 0;

    while (!blockTicks_.empty() && processed < kMaxBlockTicksPerFrame) {
        const ScheduledBlockTick tick = blockTicks_.top();
        if (tick.dueTime > gameTimeSeconds_) {
            break;
        }
        blockTicks_.pop();
        ++processed;

        const std::string key = game_internal::blockTickKey(tick.block);
        const auto generationIt = blockTickGeneration_.find(key);
        if (generationIt == blockTickGeneration_.end() || generationIt->second != tick.generation) {
            continue;
        }

        const std::uint16_t stateId = getBlock(world_, tick.block.x, tick.block.y, tick.block.z);
        const BlockDefinition* def = findBlockDefinitionForBlockType(gameData_, stateId);
        if (stateId == 0 || def == nullptr) {
            blockTickGeneration_.erase(key);
            continue;
        }

        if (game_internal::isCropBlock(*def)) {
            const std::uint16_t soil = getBlock(world_, tick.block.x, tick.block.y - 1, tick.block.z);
            const bool supported = game_internal::isCropSoil(gameData_, soil);
            const bool blockedAbove = getBlock(world_, tick.block.x, tick.block.y + 1, tick.block.z) != 0;

            if (!supported || blockedAbove) {
                setBlock(world_, tick.block.x, tick.block.y, tick.block.z, 0);
                for (const auto& drop : game_internal::cropDropsForState(gameData_, *def, stateId)) {
                    addItem(player_.inventory, gameData_, drop.item, drop.count);
                }
                rebuildMeshesAroundBlock(tick.block);
                blockTickGeneration_.erase(key);
                continue;
            }

            const int currentAge = game_internal::cropAge(gameData_, stateId);
            if (currentAge < game_internal::cropMaxAge(*def)) {
                std::uniform_real_distribution<float> chance(0.0f, 1.0f);
                if (chance(rng_) <= 0.18f) {
                    const auto nextId = runtimeIdForBlockState(gameData_, def->id, {{"age", currentAge + 1}});
                    if (nextId.has_value()) {
                        setBlock(world_, tick.block.x, tick.block.y, tick.block.z, *nextId);
                        rebuildMeshesAroundBlock(tick.block);
                    }
                }
            }
        }

        const std::uint16_t nextStateId = getBlock(world_, tick.block.x, tick.block.y, tick.block.z);
        scheduleBlockTick(tick.block, nextStateId, 0.0f);
    }
}

void Game::scheduleBlockTick(const Int3& block, const std::uint16_t stateId, const float delaySeconds) {
    if (stateId == 0) {
        blockTickGeneration_.erase(game_internal::blockTickKey(block));
        return;
    }

    const BlockDefinition* def = findBlockDefinitionForBlockType(gameData_, stateId);
    if (def == nullptr) {
        return;
    }

    const auto intervalSeconds = game_internal::tickIntervalSeconds(*def);
    if (!intervalSeconds.has_value()) {
        blockTickGeneration_.erase(game_internal::blockTickKey(block));
        return;
    }

    const std::string key = game_internal::blockTickKey(block);
    const std::uint32_t generation = ++blockTickGeneration_[key];
    const double dueTime = gameTimeSeconds_ + (delaySeconds > 0.0f ? delaySeconds : *intervalSeconds);
    blockTicks_.push({dueTime, block, generation});
}

void Game::scheduleChunkBlockTicks(const Chunk& chunk) {
    const int baseX = chunk.coord.x * kChunkX;
    const int baseY = chunk.coord.y * kChunkY;
    const int baseZ = chunk.coord.z * kChunkZ;

    for (int lx = 0; lx < kChunkX; ++lx) {
        for (int ly = 0; ly < kChunkY; ++ly) {
            for (int lz = 0; lz < kChunkZ; ++lz) {
                const std::uint16_t stateId = chunk.blocks[lx][ly][lz];
                if (stateId == 0) continue;
                scheduleBlockTick({baseX + lx, baseY + ly, baseZ + lz}, stateId, 0.0f);
            }
        }
    }
}

void Game::launchMeshBuild(const ChunkCoord& coord) {
    World snap;
    for (const ChunkCoord& c : {
            coord,
            ChunkCoord{coord.x - 1, coord.y, coord.z},
            ChunkCoord{coord.x + 1, coord.y, coord.z},
            ChunkCoord{coord.x, coord.y - 1, coord.z},
            ChunkCoord{coord.x, coord.y + 1, coord.z},
            ChunkCoord{coord.x, coord.y, coord.z - 1},
            ChunkCoord{coord.x, coord.y, coord.z + 1}}) {
        const auto it = world_.chunks.find(c);
        if (it != world_.chunks.end()) {
            snap.chunks[c] = it->second;
        }
    }

    pendingMeshes_.push_back({coord, std::async(std::launch::async,
        [coord, s = std::move(snap), &gd = gameData_, &mm = modelManager_]() {
            return buildChunkMesh(s, coord, gd, mm);
        }
    )});
}

void Game::simulateLiquids(const float deltaTime) {
    constexpr float kLiquidStepSeconds = 0.20f;
    liquidStepAccumulator_ += deltaTime;
    if (liquidStepAccumulator_ < kLiquidStepSeconds) {
        return;
    }
    liquidStepAccumulator_ = 0.0f;

    struct LiquidMove {
        Int3 from;
        Int3 to;
        std::uint16_t blockType;
    };

    std::vector<LiquidMove> moves;
    moves.reserve(64);

    for (const auto& [coord, chunk] : world_.chunks) {
        const int baseX = coord.x * kChunkX;
        const int baseY = coord.y * kChunkY;
        const int baseZ = coord.z * kChunkZ;
        const int yStart = (baseY == 0) ? 1 : 0;

        for (int lx = 0; lx < kChunkX; ++lx) {
            for (int ly = yStart; ly < kChunkY; ++ly) {
                for (int lz = 0; lz < kChunkZ; ++lz) {
                    const int wx = baseX + lx;
                    const int wy = baseY + ly;
                    const int wz = baseZ + lz;

                    const std::uint16_t blockType = chunk.blocks[lx][ly][lz];
                    if (blockType == 0 || !isLiquidBlockType(gameData_, blockType)) {
                        continue;
                    }
                    if (isOccupied(world_, wx, wy - 1, wz)) {
                        continue;
                    }
                    moves.push_back({{wx, wy, wz}, {wx, wy - 1, wz}, blockType});
                }
            }
        }
    }

    std::unordered_set<ChunkCoord, ChunkCoordHash> dirtyChunks;
    const auto markDirty = [&](const Int3& block) {
        const ChunkCoord center = worldToChunkCoord(block.x, block.y, block.z);
        const Int3 local = worldToLocalBlock(block.x, block.y, block.z);
        dirtyChunks.insert(center);
        if (local.x == 0) dirtyChunks.insert({center.x - 1, center.y, center.z});
        if (local.x == kChunkX - 1) dirtyChunks.insert({center.x + 1, center.y, center.z});
        if (local.y == 0) dirtyChunks.insert({center.x, center.y - 1, center.z});
        if (local.y == kChunkY - 1) dirtyChunks.insert({center.x, center.y + 1, center.z});
        if (local.z == 0) dirtyChunks.insert({center.x, center.y, center.z - 1});
        if (local.z == kChunkZ - 1) dirtyChunks.insert({center.x, center.y, center.z + 1});
    };

    for (const auto& move : moves) {
        if (getBlock(world_, move.from.x, move.from.y, move.from.z) != move.blockType) {
            continue;
        }
        if (isOccupied(world_, move.to.x, move.to.y, move.to.z)) {
            continue;
        }

        setBlock(world_, move.from.x, move.from.y, move.from.z, 0);
        setBlock(world_, move.to.x, move.to.y, move.to.z, move.blockType);
        markDirty(move.from);
        markDirty(move.to);
    }

    for (const auto& coord : dirtyChunks) {
        rebuildChunkMesh(coord);
    }
}

void Game::updateLoadedChunks(const ChunkCoord& playerChunk) {
    constexpr int kMaxQueuesPerFrame = 4;
    int queued = 0;
    for (int dz = -kViewDistance; dz <= kViewDistance && queued < kMaxQueuesPerFrame; ++dz) {
        for (int dx = -kViewDistance; dx <= kViewDistance && queued < kMaxQueuesPerFrame; ++dx) {
            for (int cy = 0; cy < kChunkCountY && queued < kMaxQueuesPerFrame; ++cy) {
                const ChunkCoord coord {playerChunk.x + dx, cy, playerChunk.z + dz};
                if (chunkLoaded(world_, coord)) continue;
                if (pendingTerrain_.find(coord) != pendingTerrain_.end()) continue;

                const TerrainGenerator gen = terrainGen_;
                pendingTerrain_[coord] = std::async(std::launch::async,
                    [coord, gen, &gd = gameData_]() {
                        return gen.generateChunk(coord, gd);
                    }
                );
                ++queued;
            }
        }
    }

    const int unloadDist = kViewDistance + 2;
    std::vector<ChunkCoord> toUnload;
    for (const auto& [coord, chunk] : world_.chunks) {
        if (std::abs(coord.x - playerChunk.x) > unloadDist ||
            std::abs(coord.z - playerChunk.z) > unloadDist) {
            toUnload.push_back(coord);
        }
    }
    for (const auto& coord : toUnload) {
        pendingTerrain_.erase(coord);
        pendingMeshes_.erase(
            std::remove_if(pendingMeshes_.begin(), pendingMeshes_.end(),
                [&coord](PendingMesh& pm) { return pm.coord == coord; }),
            pendingMeshes_.end()
        );
        world_.chunks.erase(coord);
        const auto it = meshes_.find(coord);
        if (it != meshes_.end()) {
            destroyChunkMesh(it->second);
            meshes_.erase(it);
        }
    }
}

}  // namespace voxel
