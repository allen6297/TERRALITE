#include "game/Game.hpp"

#include <algorithm>
#include <chrono>
#include <unordered_set>
#include <vector>

#include "render/Mesh.hpp"
#include "render/Renderer.hpp"
#include "world/Chunk.hpp"

namespace voxel {
// Populate block.faceTextures from its associated model and load any new
// textures into the texture manager.  Call after modelManager_ is populated.
void Game::populateFaceTextures() {
    static constexpr std::array<const char*, 6> kFaces {
        "up", "down", "north", "south", "east", "west"
    };

    for (auto& [blockId, block] : gameData_.blocks) {
        if (block.modelPath.empty()) continue;
        const BlockModel* model = modelManager_.get(block.modelPath);
        if (!model || model->textures.empty()) continue;

        block.faceTextures.clear();

        const auto setFace = [&](const std::string& face, const std::string& path) {
            block.faceTextures[face] = path;
            textureManager_.loadTexture(path, assetsRoot_);
            // Also set albedo as a fallback for any code still using block.textures
            if (!block.textures.albedo.has_value()) {
                block.textures.albedo = path;
            }
        };

        // Derive collision boxes from model elements (0-16 → 0-1 space)
        block.collisionBoxes.clear();
        for (const auto& element : model->elements) {
            block.collisionBoxes.push_back({
                element.from[0] / 16.0f, element.from[1] / 16.0f, element.from[2] / 16.0f,
                element.to[0]   / 16.0f, element.to[1]   / 16.0f, element.to[2]   / 16.0f
            });
        }

        // Track the maximum positive extent across all boxes to determine how
        // far outside the [0,1] unit cube elements can reach.  Used to widen
        // the collision search range in playerCollidesAt.
        for (const auto& box : block.collisionBoxes) {
            const float maxExtent = std::max({box.maxX, box.maxY, box.maxZ});
            const int needed = std::max(1, static_cast<int>(std::ceil(maxExtent - 1.0f)));
            if (needed > gameData_.collisionSearchExpansion) {
                gameData_.collisionSearchExpansion = needed;
            }
        }

        for (const auto& [key, path] : model->textures) {
            if (key == "all") {
                for (const char* f : kFaces) setFace(f, path);
            } else if (key == "top" || key == "up" || key == "end") {
                setFace("up", path); setFace("down", path);
            } else if (key == "bottom" || key == "down") {
                setFace("down", path);
            } else if (key == "side") {
                setFace("north", path); setFace("south", path);
                setFace("east",  path); setFace("west",  path);
            } else {
                for (const char* f : kFaces) {
                    if (key == f) { setFace(key, path); break; }
                }
            }
        }
    }
}

Game::Game(GameData gameData, std::string assetsRoot, std::string dataRoot)
    : assetsRoot_(std::move(assetsRoot)), dataRoot_(std::move(dataRoot)), gameData_(std::move(gameData)) {
    for (const auto& [blockId, block] : gameData_.blocks) {
        if (!block.modelPath.empty()) {
            modelManager_.load(block.modelPath, assetsRoot_);
        }
    }
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
    populateFaceTextures();
}

Game::~Game() {
    // Wait for async work to finish before members are destroyed
    pendingTerrain_.clear();
    pendingMeshes_.clear();
    for (auto& [coord, mesh] : meshes_) destroyChunkMesh(mesh);
    for (auto& [id,    mesh] : iconMeshes_) destroyChunkMesh(mesh);
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

    // F5 hot reload
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
        drawHoveredFaceOverlay(*currentHit_);
    }
    const float wx = player_.position.x;
    const float wz = player_.position.z;
    const ClimatePoint climate = terrainGen_.sampleClimateAt(wx, wz);
    const BiomeDefinition* biome = terrainGen_.selectBiomeAt(wx, wz, gameData_.biomes);

}

DebugOverlayData Game::getDebugData() const {
    const int px = static_cast<int>(std::floor(player_.position.x));
    const int py = std::clamp(static_cast<int>(std::floor(player_.position.y)), 0, kWorldY - 1);
    const int pz = static_cast<int>(std::floor(player_.position.z));
    const ChunkCoord playerChunk = worldToChunkCoord(px, py, pz);

    static const ChunkMesh kEmptyMesh {};
    const auto meshIt = meshes_.find(playerChunk);
    const ChunkMesh& activeMesh = (meshIt != meshes_.end()) ? meshIt->second : kEmptyMesh;

    const float wx = player_.position.x;
    const float wz = player_.position.z;
    const ClimatePoint climate = terrainGen_.sampleClimateAt(wx, wz);
    const BiomeDefinition* biome = terrainGen_.selectBiomeAt(wx, wz, gameData_.biomes);

    return {
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
        activeMesh.triangleCount,
        biome ? biome->name : "",
        climate.temperature,
        climate.humidity,
        climate.rainfall,
        climate.elevation,
        climate.drainage,
        climate.waterTable
    };
}

void Game::reloadGameData() {
    // Wait for any in-flight async work to finish before swapping data
    pendingTerrain_.clear();
    pendingMeshes_.clear();
    for (auto& [id, mesh] : iconMeshes_) destroyChunkMesh(mesh);
    iconMeshes_.clear();

    gameData_ = loadGameData(dataRoot_);

    // Reload model manager with updated block definitions
    modelManager_ = ModelManager{};
    for (const auto& [blockId, block] : gameData_.blocks) {
        if (!block.modelPath.empty()) {
            modelManager_.load(block.modelPath, assetsRoot_);
        }
    }
    populateFaceTextures();

    // Destroy existing meshes and trigger rebuilds with the new data
    std::vector<ChunkCoord> coordsToRebuild;
    coordsToRebuild.reserve(meshes_.size());
    for (auto& [coord, mesh] : meshes_) {
        coordsToRebuild.push_back(coord);
        destroyChunkMesh(mesh);
    }
    meshes_.clear();
    for (const ChunkCoord& coord : coordsToRebuild) {
        launchMeshBuild(coord);
    }
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
    if (isObstructedByModel(world_, gameData_, target)) {
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

    // Temporarily place the block, run the same collidesAt check used for player
    // movement, then undo — ensures placement uses identical collision logic.
    setBlock(world_, target.x, target.y, target.z, *blockType);
    const bool wouldCollide = playerCollidesAt(world_, gameData_, player_.position);
    setBlock(world_, target.x, target.y, target.z, 0);
    if (wouldCollide) {
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
    ChunkMesh newMesh = buildChunkMesh(world_, coord, gameData_, modelManager_);
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

void Game::renderHotbarIcons(int fbWidth, int fbHeight) {
    // Layout constants — must match hud.rcss exactly
    constexpr float kSlotOuter  = 52.0f;
    constexpr float kGap        = 6.0f;
    constexpr float kBorder     = 2.0f;
    constexpr float kBottomOff  = 14.0f;
    constexpr float kStep       = kSlotOuter + kGap;
    constexpr float kContainerW = (kSlotOuter + kGap) * static_cast<float>(kInventorySlots);

    const float containerLeft = static_cast<float>(fbWidth)  * 0.5f - kContainerW * 0.5f;
    const float slotTop       = static_cast<float>(fbHeight) - kBottomOff - kSlotOuter;
    const float inner         = kSlotOuter - kBorder * 2.0f;  // 48 px

    // Save full GL state and current viewport
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    GLint savedVP[4];
    glGetIntegerv(GL_VIEWPORT, savedVP);

    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    for (int i = 0; i < kInventorySlots; ++i) {
        // Slot interior in screen coords (Y-down), converted to GL coords (Y-up)
        const int vx = static_cast<int>(containerLeft + static_cast<float>(i) * kStep + kBorder);
        const int vy_screen = static_cast<int>(slotTop + kBorder);
        const int vw = static_cast<int>(inner);
        const int vh = static_cast<int>(inner);
        const int vy_gl = fbHeight - vy_screen - vh + 2;

        glScissor(vx, vy_gl, vw, vh + 1);
        glViewport(vx, vy_gl, vw, vh + 1);

        // Slot background — light beige matching the original hotbar aesthetic
        glClearColor(0.84f, 0.82f, 0.80f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const auto& slot = player_.inventory.slots[static_cast<std::size_t>(i)];
        if (slot.count == 0 || slot.itemId.empty()) continue;

        // Resolve item → block
        const BlockDefinition* block = nullptr;
        if (const auto* item = findItemDefinition(gameData_, slot.itemId)) {
            if (item->placeableBlock.has_value())
                block = findBlockDefinition(gameData_, *item->placeableBlock);
        }
        if (!block) block = findBlockDefinition(gameData_, slot.itemId);
        if (!block) continue;

        // Build icon mesh on first use (lazy, main-thread GPU upload)
        if (!iconMeshes_.contains(block->id)) {
            // Fake world: one block at local (1,1,1) surrounded by air
            World fakeWorld;
            Chunk chunk;
            chunk.blocks[1][1][1] = runtimeIdForBlock(gameData_, block->id);
            fakeWorld.chunks[{0, 0, 0}] = chunk;

            ChunkMesh mesh = buildChunkMesh(fakeWorld, {0, 0, 0}, gameData_, modelManager_);
            uploadChunkMesh(mesh);
            iconMeshes_.emplace(block->id, std::move(mesh));
        }
        const ChunkMesh& iconMesh = iconMeshes_.at(block->id);
        if (iconMesh.surfaces.empty()) continue;

        // Dynamic ortho extent — fit projection to this model's actual geometry.
        // Block lives at world (1,1,1); model space is 0-16.
        float cx = 1.5f, cy = 1.5f, cz = 1.5f;
        float orthoExt = 0.95f;

        if (const BlockModel* model = modelManager_.get(block->modelPath);
            model && !model->elements.empty())
        {
            float bmin[3] = {16,16,16}, bmax[3] = {0,0,0};
            for (const auto& el : model->elements) {
                for (int a = 0; a < 3; ++a) {
                    bmin[a] = std::min(bmin[a], el.from[a]);
                    bmax[a] = std::max(bmax[a], el.to[a]);
                }
            }
            // Centre of model in world space
            cx = 1.0f + (bmin[0] + bmax[0]) / 32.0f;
            cy = 1.0f + (bmin[1] + bmax[1]) / 32.0f;
            cz = 1.0f + (bmin[2] + bmax[2]) / 32.0f;

            // Half-extents around that centre
            const float hx = (bmax[0] - bmin[0]) / 32.0f;
            const float hy = (bmax[1] - bmin[1]) / 32.0f;
            const float hz = (bmax[2] - bmin[2]) / 32.0f;

            // Projection constants matching the view rotations below:
            //   R_y(-45°) then R_x(+28°)
            //   screen_x = (lx - lz) * cos45
            //   screen_y = ly*cos28 - (lx+lz)*cos45*sin28
            static constexpr float kC45 = 0.70711f;
            static constexpr float kC28 = 0.88295f;
            static constexpr float kS28 = 0.46947f;

            float maxE = 0.0f;
            for (int sx : {-1, 1}) for (int sy : {-1, 1}) for (int sz : {-1, 1}) {
                const float lx = sx * hx, ly = sy * hy, lz = sz * hz;
                const float scrX =  (lx - lz) * kC45;
                const float scrY =   ly * kC28 - (lx + lz) * kC45 * kS28;
                maxE = std::max(maxE, std::max(std::abs(scrX), std::abs(scrY)));
            }
            orthoExt = std::max(maxE * 1.18f, 0.05f);  // 18 % breathing room
        }

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-orthoExt, orthoExt, -orthoExt, orthoExt, 0.1, 20.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, -0.05f, -5.0f);   // tiny downward bias
        glRotatef(28.0f,  1.0f, 0.0f, 0.0f);
        glRotatef(-45.0f, 0.0f, 1.0f, 0.0f);
        glTranslatef(-cx, -cy, -cz);          // centre on model bounding box

        renderMesh(iconMesh, textureManager_);
    }

    // Restore everything
    glDisable(GL_SCISSOR_TEST);
    glViewport(savedVP[0], savedVP[1], savedVP[2], savedVP[3]);
    glPopAttrib();
}

}  // namespace voxel
