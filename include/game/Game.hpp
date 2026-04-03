#pragma once

#include <future>
#include <unordered_map>
#include <vector>

#include <GLFW/glfw3.h>

#include "data/GameData.hpp"
#include "Player.hpp"
#include "render/Mesh.hpp"
#include "render/ModelManager.hpp"
#include "render/Renderer.hpp"
#include "render/TextureManager.hpp"
#include "world/TerrainGenerator.hpp"
#include "world/World.hpp"

namespace voxel {
class Game {
public:
    Game(GameData gameData, std::string assetsRoot, std::string dataRoot);
    ~Game();
    void update(GLFWwindow* window, float deltaTime);
    void render(int framebufferWidth, int framebufferHeight) const;
    void renderHotbarIcons(int framebufferWidth, int framebufferHeight);
    DebugOverlayData getDebugData() const;
    const Inventory& getInventory() const { return player_.inventory; }

private:
    struct PendingMesh {
        ChunkCoord coord;
        std::future<ChunkMesh> future;
    };

    void simulateLiquids(float deltaTime);
    void handleBlockActions();
    void handleInventorySelection(GLFWwindow* window);
    void rebuildChunkMesh(const ChunkCoord& coord);
    void rebuildMeshesAroundBlock(const Int3& block);
    void updateLoadedChunks(const ChunkCoord& playerChunk);
    void launchMeshBuild(const ChunkCoord& coord);
    void collectPending();
    void reloadGameData();
    void populateFaceTextures();

    std::string assetsRoot_;
    std::string dataRoot_;
    bool f5WasPressed_ = false;
    GameData gameData_;
    TextureManager textureManager_;
    ModelManager modelManager_;
    TerrainGenerator terrainGen_;
    World world_;
    std::unordered_map<ChunkCoord, ChunkMesh, ChunkCoordHash> meshes_;
    std::unordered_map<ChunkCoord, std::future<Chunk>, ChunkCoordHash> pendingTerrain_;
    std::unordered_map<std::string, ChunkMesh> iconMeshes_;  // cached single-block meshes for hotbar icons
    std::vector<PendingMesh> pendingMeshes_;
    Player player_;
    InputState input_;
    std::optional<RaycastHit> currentHit_;
    float liquidStepAccumulator_ = 0.0f;
    float fpsAccumulator_ = 0.0f;
    int fpsFrameCount_ = 0;
    int fps_ = 0;
    float frameTimeMs_ = 0.0f;
};
}  // namespace voxel
