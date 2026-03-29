#include "game/Game.hpp"

#include <chrono>
#include <unordered_set>
#include <vector>

#include "render/Renderer.hpp"

namespace voxel {
Game::Game(GameData gameData, std::string assetsRoot)
    : assetsRoot_(std::move(assetsRoot)), gameData_(std::move(gameData)) {
    textureManager_.loadTexture(kHoverFaceTexturePath, assetsRoot_);
    for (const auto& [blockId, block] : gameData_.blocks) {
        for (const auto& texturePath : {
                 texturePathForType(block, "albedo"),
                 texturePathForType(block, "normal"),
                 texturePathForType(block, "roughness"),
                 texturePathForType(block, "emissive")
             }) {
            if (texturePath.has_value()) {
                textureManager_.loadTexture(*texturePath, assetsRoot_);
            }
        }
    }
}

Game::~Game() {
    // Wait for async work to finish before members are destroyed
    pendingTerrain_.clear();
    pendingMeshes_.clear();
    for (auto& [coord, mesh] : meshes_) {
        destroyChunkMesh(mesh);
    }
}

void Game::update(GLFWwindow* window, const float deltaTime) {
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
    currentHit_ = raycastWorld(world_, gameData_, getEyePosition(player_), getLookDirection(player_));
    handleBlockActions();

    const int px = static_cast<int>(std::floor(player_.position.x));
    const int py = std::clamp(static_cast<int>(std::floor(player_.position.y)), 0, kWorldY - 1);
    const int pz = static_cast<int>(std::floor(player_.position.z));
    updateLoadedChunks(worldToChunkCoord(px, py, pz));
}

void Game::render(const int framebufferWidth, const int framebufferHeight) const {
    const float aspect = framebufferHeight == 0 ? 1.0f :
        static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);

    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClearColor(0.58f, 0.78f, 0.98f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    setPerspective(70.0f, aspect, 0.1f, 200.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const Vec3 eye = getEyePosition(player_);
    applyCameraView(eye, getLookDirection(player_));

    const int px = static_cast<int>(std::floor(player_.position.x));
    const int py = std::clamp(static_cast<int>(std::floor(player_.position.y)), 0, kWorldY - 1);
    const int pz = static_cast<int>(std::floor(player_.position.z));
    const ChunkCoord playerChunk = worldToChunkCoord(px, py, pz);

    for (const auto& [coord, mesh] : meshes_) {
        if (mesh.triangleCount > 0) {
            renderMesh(mesh, textureManager_);
        }
    }
    if (const auto it = meshes_.find(playerChunk); it != meshes_.end()) {
        renderMeshWireframe(it->second);
    }

    static const ChunkMesh kEmptyMesh {};
    const auto meshIt = meshes_.find(playerChunk);
    const ChunkMesh& activeMesh = (meshIt != meshes_.end()) ? meshIt->second : kEmptyMesh;

    drawCrosshair(framebufferWidth, framebufferHeight);
    if (currentHit_.has_value()) {
        drawHoveredFaceOverlay(*currentHit_, textureManager_, kHoverFaceTexturePath);
    }
    drawInventoryBar(framebufferWidth, framebufferHeight, player_.inventory);
    drawDebugOverlay(framebufferWidth, framebufferHeight, {
        player_.position,
        currentHit_.has_value() ? std::optional<Int3>(currentHit_->block) : std::nullopt,
        fps_,
        frameTimeMs_,
        playerChunk.x,
        playerChunk.y,
        playerChunk.z,
        static_cast<int>(meshes_.size()),
        activeMesh.solidBlocks,
        activeMesh.visibleFaces,
        activeMesh.triangleCount
    });
}

void Game::collectPending() {
    // Collect finished terrain generation, launch mesh builds
    for (auto it = pendingTerrain_.begin(); it != pendingTerrain_.end(); ) {
        if (it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const ChunkCoord coord = it->first;
            world_.chunks[coord] = it->second.get();
            it = pendingTerrain_.erase(it);
            launchMeshBuild(coord);
        } else {
            ++it;
        }
    }

    // Collect finished mesh builds, upload to GPU on main thread
    for (auto it = pendingMeshes_.begin(); it != pendingMeshes_.end(); ) {
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

void Game::launchMeshBuild(const ChunkCoord& coord) {
    // Snapshot the center chunk and its 6 face neighbors so the async task
    // has no shared state with the main thread's world_.
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
        [coord, s = std::move(snap), &gd = gameData_]() {
            return buildChunkMesh(s, coord, gd);
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
        std::uint8_t blockType;
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

                    const std::uint8_t blockType = getBlock(world_, wx, wy, wz);
                    if (!isLiquidBlockType(gameData_, blockType)) {
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
        if (local.x == 0)           dirtyChunks.insert({center.x - 1, center.y, center.z});
        if (local.x == kChunkX - 1) dirtyChunks.insert({center.x + 1, center.y, center.z});
        if (local.y == 0)           dirtyChunks.insert({center.x, center.y - 1, center.z});
        if (local.y == kChunkY - 1) dirtyChunks.insert({center.x, center.y + 1, center.z});
        if (local.z == 0)           dirtyChunks.insert({center.x, center.y, center.z - 1});
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

void Game::handleBlockActions() {
    if (!currentHit_.has_value()) {
        return;
    }

    const BlockDefinition* hitDef = findBlockDefinitionForBlockType(gameData_, currentHit_->type);
    if (input_.breakPressed && (hitDef == nullptr || hitDef->material != "liquid")) {
        const BlockDefinition* block = findBlockDefinitionForBlockType(gameData_, currentHit_->type);
        setBlock(world_, currentHit_->block.x, currentHit_->block.y, currentHit_->block.z, 0);
        if (block != nullptr) {
            for (const auto& drop : block->drops) {
                addItem(player_.inventory, gameData_, drop.item, drop.count);
            }
        }
        rebuildMeshesAroundBlock(currentHit_->block);
    }

    if (!input_.placePressed) {
        return;
    }

    const Int3 target = currentHit_->previousEmpty;
    if (!isYInBounds(target.y)) {
        return;
    }
    if (isOccupied(world_, target.x, target.y, target.z)) {
        return;
    }
    if (blockWouldIntersectPlayer(target, player_)) {
        return;
    }

    const auto selectedItem = selectedItemId(player_.inventory);
    if (!selectedItem.has_value()) {
        return;
    }

    const auto blockType = blockTypeForItemId(gameData_, *selectedItem);
    if (!blockType.has_value()) {
        return;
    }
    if (!removeSelectedItem(player_.inventory, 1)) {
        return;
    }

    setBlock(world_, target.x, target.y, target.z, *blockType);
    rebuildMeshesAroundBlock(target);
}

void Game::handleInventorySelection(GLFWwindow* window) {
    for (int i = 0; i < kInventorySlots; ++i) {
        const int key = GLFW_KEY_1 + i;
        const bool down = glfwGetKey(window, key) == GLFW_PRESS;
        if (down && !input_.slotHeld[static_cast<std::size_t>(i)]) {
            selectSlot(player_.inventory, i);
        }
        input_.slotHeld[static_cast<std::size_t>(i)] = down;
    }
}

void Game::rebuildChunkMesh(const ChunkCoord& coord) {
    if (!inChunkBounds(coord) || !chunkLoaded(world_, coord)) {
        return;
    }
    // Cancel any in-flight async mesh build for this coord so it can't overwrite us
    pendingMeshes_.erase(
        std::remove_if(pendingMeshes_.begin(), pendingMeshes_.end(),
            [&coord](PendingMesh& pm) { return pm.coord == coord; }),
        pendingMeshes_.end()
    );
    const auto it = meshes_.find(coord);
    if (it != meshes_.end()) {
        destroyChunkMesh(it->second);
    }
    ChunkMesh newMesh = buildChunkMesh(world_, coord, gameData_);
    uploadChunkMesh(newMesh);
    meshes_[coord] = std::move(newMesh);
}

void Game::rebuildMeshesAroundBlock(const Int3& block) {
    const ChunkCoord center = worldToChunkCoord(block.x, block.y, block.z);
    rebuildChunkMesh(center);

    const Int3 local = worldToLocalBlock(block.x, block.y, block.z);
    if (local.x == 0)           rebuildChunkMesh({center.x - 1, center.y, center.z});
    if (local.x == kChunkX - 1) rebuildChunkMesh({center.x + 1, center.y, center.z});
    if (local.y == 0)           rebuildChunkMesh({center.x, center.y - 1, center.z});
    if (local.y == kChunkY - 1) rebuildChunkMesh({center.x, center.y + 1, center.z});
    if (local.z == 0)           rebuildChunkMesh({center.x, center.y, center.z - 1});
    if (local.z == kChunkZ - 1) rebuildChunkMesh({center.x, center.y, center.z + 1});
}

void Game::updateLoadedChunks(const ChunkCoord& playerChunk) {
    // Queue async terrain generation for chunks within view distance
    constexpr int kMaxQueuesPerFrame = 4;
    int queued = 0;
    for (int dz = -kViewDistance; dz <= kViewDistance && queued < kMaxQueuesPerFrame; ++dz) {
        for (int dx = -kViewDistance; dx <= kViewDistance && queued < kMaxQueuesPerFrame; ++dx) {
            for (int cy = 0; cy < kChunkCountY && queued < kMaxQueuesPerFrame; ++cy) {
                const ChunkCoord coord {playerChunk.x + dx, cy, playerChunk.z + dz};
                if (chunkLoaded(world_, coord)) {
                    continue;
                }
                if (pendingTerrain_.find(coord) != pendingTerrain_.end()) {
                    continue;
                }
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

    // Unload chunks that have moved out of range
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
