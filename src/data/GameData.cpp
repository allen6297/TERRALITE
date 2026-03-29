#include "data/GameData.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

#include "data/JsonValue.hpp"

namespace voxel {
namespace {
constexpr std::array<const char*, 4> kPreferredBlockOrder = {
    "grass",
    "dirt",
    "water",
    "stone"
};

const JsonValue& getRequiredField(const JsonValue::Object& object, const std::string& key) {
    const auto iterator = object.find(key);
    if (iterator == object.end()) {
        throw std::runtime_error("Missing required JSON field: " + key);
    }
    return iterator->second;
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

std::array<float, 3> readColor(const JsonValue& value) {
    const auto& array = value.asArray();
    if (array.size() != 3) {
        throw std::runtime_error("Color must contain exactly 3 numbers");
    }
    return {
        static_cast<float>(array[0].asNumber()),
        static_cast<float>(array[1].asNumber()),
        static_cast<float>(array[2].asNumber())
    };
}

BlockTextures readTextures(const JsonValue& value) {
    BlockTextures textures;

    if (value.isString()) {
        textures.albedo = value.asString();
        return textures;
    }

    const auto& object = value.asObject();
    const auto assignIfPresent = [&object](const std::string& key, std::optional<std::string>& target) {
        const auto iterator = object.find(key);
        if (iterator != object.end() && iterator->second.isString()) {
            target = iterator->second.asString();
        }
    };

    assignIfPresent("albedo", textures.albedo);
    assignIfPresent("normal", textures.normal);
    assignIfPresent("roughness", textures.roughness);
    assignIfPresent("emissive", textures.emissive);
    return textures;
}

BlockDefinition parseBlockDefinition(const JsonValue& value) {
    const auto& object = value.asObject();
    const auto& voxel = getRequiredField(object, "voxel").asObject();
    const auto& render = getRequiredField(object, "render").asObject();

    BlockDefinition definition;
    definition.id = getRequiredField(object, "id").asString();
    definition.name = getRequiredField(object, "name").asString();
    definition.solid = getRequiredField(voxel, "solid").asBool();
    const auto translucentIterator = voxel.find("translucent");
    definition.translucent = translucentIterator != voxel.end() && translucentIterator->second.asBool();
    definition.material = getRequiredField(voxel, "material").asString();
    definition.color = readColor(getRequiredField(render, "color"));
    const auto opacityIterator = render.find("opacity");
    if (opacityIterator != render.end() && opacityIterator->second.isNumber()) {
        definition.opacity = static_cast<float>(opacityIterator->second.asNumber());
    }
    definition.textures = readTextures(getRequiredField(render, "texture"));

    for (const auto& dropValue : getRequiredField(object, "drops").asArray()) {
        const auto& dropObject = dropValue.asObject();
        definition.drops.push_back({
            getRequiredField(dropObject, "item").asString(),
            static_cast<int>(getRequiredField(dropObject, "count").asNumber())
        });
    }

    return definition;
}

ItemDefinition parseItemDefinition(const JsonValue& value) {
    const auto& object = value.asObject();

    ItemDefinition definition;
    definition.id = getRequiredField(object, "id").asString();
    definition.name = getRequiredField(object, "name").asString();
    definition.stackSize = static_cast<int>(getRequiredField(object, "stackSize").asNumber());
    definition.icon = getRequiredField(object, "icon").asString();

    const auto placeableIterator = object.find("placeableBlock");
    if (placeableIterator != object.end() && placeableIterator->second.isString()) {
        definition.placeableBlock = placeableIterator->second.asString();
    }

    return definition;
}

template <typename T, typename Parser>
void loadDefinitions(
    const std::filesystem::path& directory,
    std::unordered_map<std::string, T>& destination,
    Parser parser
) {
    if (!std::filesystem::exists(directory)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }

        const JsonValue document = parseJson(readTextFile(entry.path()));
        T definition = parser(document);
        destination.emplace(definition.id, std::move(definition));
    }
}
}  // namespace

GameData loadGameData(const std::string& dataRoot) {
    GameData data;
    const std::filesystem::path root(dataRoot);

    if (!std::filesystem::exists(root)) {
        throw std::runtime_error("Data root does not exist: " + root.string());
    }

    loadDefinitions<BlockDefinition>(root / "blocks", data.blocks, parseBlockDefinition);
    loadDefinitions<ItemDefinition>(root / "items", data.items, parseItemDefinition);

    std::map<std::string, std::uint8_t> assignedIds;
    std::uint8_t nextId = 1;

    for (const char* preferredId : kPreferredBlockOrder) {
        if (!data.blocks.contains(preferredId)) {
            continue;
        }
        if (nextId == 0) {
            throw std::runtime_error("Ran out of runtime block ids");
        }
        assignedIds.emplace(preferredId, nextId++);
    }

    for (const auto& [blockId, block] : data.blocks) {
        (void)block;
        if (assignedIds.contains(blockId)) {
            continue;
        }
        if (nextId == 0) {
            throw std::runtime_error("Ran out of runtime block ids");
        }
        assignedIds.emplace(blockId, nextId++);
    }

    for (auto& [blockId, block] : data.blocks) {
        const auto assigned = assignedIds.find(blockId);
        block.runtimeId = assigned->second;
        data.blockIdsByRuntimeId.emplace(block.runtimeId, blockId);
        data.solidByRuntimeId[block.runtimeId]  = block.solid;
        data.liquidByRuntimeId[block.runtimeId] = (block.material == "liquid");
    }

    return data;
}

std::uint8_t runtimeIdForBlock(const GameData& gameData, const std::string& blockId) {
    const auto it = gameData.blocks.find(blockId);
    return it != gameData.blocks.end() ? it->second.runtimeId : 0;
}

const BlockDefinition* findBlockDefinitionForBlockType(const GameData& gameData, const std::uint8_t blockType) {
    const auto idIterator = gameData.blockIdsByRuntimeId.find(blockType);
    if (idIterator == gameData.blockIdsByRuntimeId.end()) {
        return nullptr;
    }

    const auto iterator = gameData.blocks.find(idIterator->second);
    if (iterator == gameData.blocks.end()) {
        return nullptr;
    }

    return &iterator->second;
}

const ItemDefinition* findItemDefinition(const GameData& gameData, const std::string& itemId) {
    const auto iterator = gameData.items.find(itemId);
    return iterator == gameData.items.end() ? nullptr : &iterator->second;
}

const BlockDefinition* findBlockDefinition(const GameData& gameData, const std::string& blockId) {
    const auto iterator = gameData.blocks.find(blockId);
    return iterator == gameData.blocks.end() ? nullptr : &iterator->second;
}

std::optional<std::uint8_t> blockTypeForItemId(const GameData& gameData, const std::string& itemId) {
    const ItemDefinition* item = findItemDefinition(gameData, itemId);
    if (item == nullptr || !item->placeableBlock.has_value()) {
        return std::nullopt;
    }

    const BlockDefinition* block = findBlockDefinition(gameData, *item->placeableBlock);
    if (block == nullptr) {
        return std::nullopt;
    }

    return block->runtimeId;
}

const std::optional<std::string>& texturePathForType(const BlockDefinition& block, const std::string& textureType) {
    if (textureType == "albedo") {
        return block.textures.albedo;
    }
    if (textureType == "normal") {
        return block.textures.normal;
    }
    if (textureType == "roughness") {
        return block.textures.roughness;
    }
    return block.textures.emissive;
}

bool isLiquidBlockType(const GameData& gameData, const std::uint8_t blockType) {
    return blockType != 0 && gameData.liquidByRuntimeId[blockType];
}
}  // namespace voxel
