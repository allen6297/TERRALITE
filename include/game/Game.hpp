#pragma once

#include <future>
#include <unordered_map>
#include <vector>

#include <GLFW/glfw3.h>

#include "data/GameData.hpp"
#include "Player.hpp"
#include "render/Mesh.hpp"
#include "render/TextureManager.hpp"
#include "world/TerrainGenerator.hpp"
#include "world/World.hpp"

namespace voxel {
class Game {
public:
    Game(GameData gameData, std::string assetsRoot);
    ~Game();
    void update(GLFWwindow* window, float deltaTime);
    void render(int framebufferWidth, int framebufferHeight) const;

private:
    static constexpr const char* kHoverFaceTexturePath = "textures/ui/hover_face.ppm";

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

    std::string assetsRoot_;
    GameData gameData_;
    TextureManager textureManager_;
    TerrainGenerator terrainGen_;
    World world_;
    std::unordered_map<ChunkCoord, ChunkMesh, ChunkCoordHash> meshes_;
    std::unordered_map<ChunkCoord, std::future<Chunk>, ChunkCoordHash> pendingTerrain_;
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
