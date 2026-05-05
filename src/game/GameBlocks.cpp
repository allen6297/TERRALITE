#include "game/Game.hpp"

#include <algorithm>

#include "GameInternal.hpp"

namespace voxel {

void Game::handleBlockActions() {
    if (!currentHit_.has_value()) {
        return;
    }

    const BlockDefinition* hitDef = findBlockDefinitionForBlockType(gameData_, currentHit_->type);
    if (input_.breakPressed && (hitDef == nullptr || hitDef->material != "liquid")) {
        const BlockDefinition* block = findBlockDefinitionForBlockType(gameData_, currentHit_->type);
        setBlock(world_, currentHit_->block.x, currentHit_->block.y, currentHit_->block.z, 0);
        blockTickGeneration_.erase(game_internal::blockTickKey(currentHit_->block));
        if (block != nullptr) {
            for (const auto& drop : game_internal::cropDropsForState(gameData_, *block, currentHit_->type)) {
                addItem(player_.inventory, gameData_, drop.item, drop.count);
            }
        }
        rebuildMeshesAroundBlock(currentHit_->block);
    }

    if (!input_.placePressed) {
        return;
    }

    const Int3 target = currentHit_->previousEmpty;
    if (!isYInBounds(target.y)) return;
    if (isOccupied(world_, target.x, target.y, target.z)) return;
    if (isObstructedByModel(world_, gameData_, target)) return;

    const auto selectedItem = selectedItemId(player_.inventory);
    if (!selectedItem.has_value()) {
        return;
    }

    if (*selectedItem == game_internal::kWheatSeedsItemId) {
        const std::uint16_t soil = getBlock(world_, target.x, target.y - 1, target.z);
        if (!game_internal::isCropSoil(gameData_, soil) ||
            getBlock(world_, target.x, target.y + 1, target.z) != 0) {
            return;
        }

        const auto cropType = runtimeIdForBlockState(gameData_, game_internal::kWheatBlockId, {{"age", 0}});
        if (!cropType.has_value()) return;
        if (!removeSelectedItem(player_.inventory, 1)) return;

        setBlock(world_, target.x, target.y, target.z, *cropType);
        scheduleBlockTick(target, *cropType, 0.0f);
        rebuildMeshesAroundBlock(target);
        return;
    }

    const auto blockType = blockTypeForItemId(gameData_, *selectedItem);
    if (!blockType.has_value()) {
        return;
    }

    setBlock(world_, target.x, target.y, target.z, *blockType);
    const bool wouldCollide = playerCollidesAt(world_, gameData_, player_.position);
    setBlock(world_, target.x, target.y, target.z, 0);
    if (wouldCollide) return;
    if (!removeSelectedItem(player_.inventory, 1)) return;

    setBlock(world_, target.x, target.y, target.z, *blockType);
    scheduleBlockTick(target, *blockType, 0.0f);
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

    queuedMeshBuilds_.erase(
        std::remove(queuedMeshBuilds_.begin(), queuedMeshBuilds_.end(), coord),
        queuedMeshBuilds_.end()
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
    if (local.x == 0) rebuildChunkMesh({center.x - 1, center.y, center.z});
    if (local.x == kChunkX - 1) rebuildChunkMesh({center.x + 1, center.y, center.z});
    if (local.y == 0) rebuildChunkMesh({center.x, center.y - 1, center.z});
    if (local.y == kChunkY - 1) rebuildChunkMesh({center.x, center.y + 1, center.z});
    if (local.z == 0) rebuildChunkMesh({center.x, center.y, center.z - 1});
    if (local.z == kChunkZ - 1) rebuildChunkMesh({center.x, center.y, center.z + 1});
}

}  // namespace voxel
