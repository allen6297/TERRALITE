#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace voxel {
struct BlockDrop {
    std::string item;
    int count = 0;
};

struct BlockTextures {
    std::optional<std::string> albedo;
    std::optional<std::string> normal;
    std::optional<std::string> roughness;
    std::optional<std::string> emissive;
};

struct BlockDefinition {
    std::uint8_t runtimeId = 0;
    std::string id;
    std::string name;
    bool solid = false;
    bool translucent = false;
    std::string material;
    std::array<float, 3> color {1.0f, 1.0f, 1.0f};
    float opacity = 1.0f;
    BlockTextures textures;
    std::vector<BlockDrop> drops;
};

struct ItemDefinition {
    std::string id;
    std::string name;
    int stackSize = 0;
    std::optional<std::string> placeableBlock;
    std::string icon;
};

struct GameData {
    std::unordered_map<std::string, BlockDefinition> blocks;
    std::unordered_map<std::string, ItemDefinition> items;
    std::unordered_map<std::uint8_t, std::string> blockIdsByRuntimeId;
    // Flat caches indexed by runtime block ID for hot-path queries (physics, raycast)
    std::array<bool, 256> solidByRuntimeId {};
    std::array<bool, 256> liquidByRuntimeId {};
};

GameData loadGameData(const std::string& dataRoot);
std::uint8_t runtimeIdForBlock(const GameData& gameData, const std::string& blockId);
const BlockDefinition* findBlockDefinitionForBlockType(const GameData& gameData, std::uint8_t blockType);
const ItemDefinition* findItemDefinition(const GameData& gameData, const std::string& itemId);
const BlockDefinition* findBlockDefinition(const GameData& gameData, const std::string& blockId);
std::optional<std::uint8_t> blockTypeForItemId(const GameData& gameData, const std::string& itemId);
const std::optional<std::string>& texturePathForType(const BlockDefinition& block, const std::string& textureType);
bool isLiquidBlockType(const GameData& gameData, std::uint8_t blockType);
}  // namespace voxel
