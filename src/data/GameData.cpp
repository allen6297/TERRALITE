#include "data/GameData.hpp"

#include <algorithm>
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

std::unordered_map<std::string, BlockProperty> readProperties(const JsonValue::Object& object) {
    std::unordered_map<std::string, BlockProperty> props;
    const auto it = object.find("properties");
    if (it == object.end() || !it->second.isObject()) {
        return props;
    }
    for (const auto& [key, val] : it->second.asObject()) {
        if (val.isBool()) {
            props[key] = val.asBool();
        } else if (val.isNumber()) {
            const double n = val.asNumber();
            if (n == static_cast<double>(static_cast<int>(n))) {
                props[key] = static_cast<int>(n);
            } else {
                props[key] = static_cast<float>(n);
            }
        } else if (val.isString()) {
            props[key] = val.asString();
        }
    }
    return props;
}

std::string blockPropertyToString(const BlockProperty& property) {
    if (std::holds_alternative<bool>(property)) {
        return std::get<bool>(property) ? "true" : "false";
    }
    if (std::holds_alternative<int>(property)) {
        return std::to_string(std::get<int>(property));
    }
    if (std::holds_alternative<float>(property)) {
        std::ostringstream stream;
        stream << std::get<float>(property);
        return stream.str();
    }
    return std::get<std::string>(property);
}

std::string canonicalStateKey(const std::unordered_map<std::string, BlockProperty>& stateValues) {
    std::vector<std::string> parts;
    parts.reserve(stateValues.size());
    for (const auto& [key, value] : stateValues) {
        parts.push_back(key + "=" + blockPropertyToString(value));
    }
    std::sort(parts.begin(), parts.end());

    std::ostringstream joined;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) {
            joined << ",";
        }
        joined << parts[i];
    }
    return joined.str();
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
    const auto textureIterator = render.find("texture");
    if (textureIterator != render.end()) {
        definition.textures = readTextures(textureIterator->second);
    }
    for (const auto& dropValue : getRequiredField(object, "drops").asArray()) {
        const auto& dropObject = dropValue.asObject();
        definition.drops.push_back({
            getRequiredField(dropObject, "item").asString(),
            static_cast<int>(getRequiredField(dropObject, "count").asNumber())
        });
    }
    definition.properties = readProperties(object);

    const auto renderTypeIt = render.find("type");
    if (renderTypeIt != render.end() && renderTypeIt->second.isString()) {
        definition.renderType = renderTypeIt->second.asString();
    }
    const auto modelIt = render.find("model");
    if (modelIt != render.end() && modelIt->second.isString()) {
        definition.modelPath = modelIt->second.asString();
    }

    const auto tintKeyIt = render.find("tintKey");
    if (tintKeyIt != render.end()) {
        definition.tintKey = tintKeyIt->second.asBool();
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

BlockStatePropDef parseStateProp(const JsonValue& value) {
    const auto& object = value.asObject();
    const std::string type = getRequiredField(object, "type").asString();
    BlockStatePropDef propDef;

    if (type == "bool") {
        const bool defaultVal = getRequiredField(object, "default").asBool();
        propDef.defaultValue = defaultVal;
        propDef.values = {defaultVal, !defaultVal};
    } else if (type == "enum") {
        const std::string defaultStr = getRequiredField(object, "default").asString();
        propDef.defaultValue = defaultStr;
        std::vector<BlockProperty> allValues;
        for (const auto& v : getRequiredField(object, "values").asArray()) {
            allValues.push_back(v.asString());
        }
        // Put default value first for consistent default-state enumeration
        const auto defaultIt = std::find(allValues.begin(), allValues.end(), BlockProperty{defaultStr});
        if (defaultIt != allValues.end()) {
            std::rotate(allValues.begin(), defaultIt, defaultIt + 1);
        }
        propDef.values = std::move(allValues);
    } else if (type == "int") {
        const int defaultVal = static_cast<int>(getRequiredField(object, "default").asNumber());
        const int minVal    = static_cast<int>(getRequiredField(object, "min").asNumber());
        const int maxVal    = static_cast<int>(getRequiredField(object, "max").asNumber());
        propDef.defaultValue = defaultVal;
        propDef.values.push_back(defaultVal);
        for (int i = minVal; i <= maxVal; ++i) {
            if (i != defaultVal) {
                propDef.values.push_back(i);
            }
        }
    } else {
        throw std::runtime_error("Unknown state property type: " + type);
    }

    return propDef;
}

BlockStateDefinition parseBlockStateDefinition(const JsonValue& value) {
    const auto& object = value.asObject();
    BlockStateDefinition stateDef;
    stateDef.id = getRequiredField(object, "id").asString();

    const auto statesIt = object.find("states");
    if (statesIt != object.end() && statesIt->second.isObject()) {
        // Use std::map so props are sorted by name — ensures identical enumeration order
        // regardless of JSON parse order or platform
        std::map<std::string, BlockStatePropDef> sorted;
        for (const auto& [propName, propValue] : statesIt->second.asObject()) {
            sorted.emplace(propName, parseStateProp(propValue));
        }
        for (auto& [name, propDef] : sorted) {
            stateDef.props.emplace_back(name, std::move(propDef));
        }
    }

    const auto variantsIt = object.find("variants");
    if (variantsIt != object.end() && variantsIt->second.isObject()) {
        for (const auto& [variantKey, variantValue] : variantsIt->second.asObject()) {
            if (!variantValue.isObject()) {
                continue;
            }
            const auto& variantObject = variantValue.asObject();
            BlockStateVariant variant;

            const auto modelIt = variantObject.find("model");
            if (modelIt != variantObject.end() && modelIt->second.isString()) {
                variant.modelPath = modelIt->second.asString();
            }

            stateDef.variants.emplace(variantKey, std::move(variant));
        }
    }

    return stateDef;
}

BiomeClimateRange parseClimateRange(const JsonValue::Object& obj, const std::string& key) {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->second.isObject()) {
        return {};
    }
    const auto& rangeObj = it->second.asObject();
    BiomeClimateRange range;
    const auto minIt = rangeObj.find("min");
    const auto maxIt = rangeObj.find("max");
    if (minIt != rangeObj.end() && minIt->second.isNumber()) {
        range.min = static_cast<float>(minIt->second.asNumber());
    }
    if (maxIt != rangeObj.end() && maxIt->second.isNumber()) {
        range.max = static_cast<float>(maxIt->second.asNumber());
    }
    return range;
}

BiomeDefinition parseBiomeDefinition(const JsonValue& value) {
    const auto& obj = value.asObject();
    BiomeDefinition def;
    def.id   = getRequiredField(obj, "id").asString();
    def.name = getRequiredField(obj, "name").asString();

    const auto priorityIt = obj.find("priority");
    if (priorityIt != obj.end() && priorityIt->second.isNumber()) {
        def.priority = static_cast<float>(priorityIt->second.asNumber());
    }
    const auto rarityIt = obj.find("rarity");
    if (rarityIt != obj.end() && rarityIt->second.isNumber()) {
        def.rarity = static_cast<float>(rarityIt->second.asNumber());
    }

    const auto climateIt = obj.find("climate");
    if (climateIt != obj.end() && climateIt->second.isObject()) {
        const auto& c = climateIt->second.asObject();
        def.climate.temperature = parseClimateRange(c, "temperature");
        def.climate.humidity    = parseClimateRange(c, "humidity");
        def.climate.rainfall    = parseClimateRange(c, "rainfall");
        def.climate.elevation   = parseClimateRange(c, "elevation");
        def.climate.drainage    = parseClimateRange(c, "drainage");
        def.climate.waterTable  = parseClimateRange(c, "waterTable");
    }

    const auto terrainIt = obj.find("terrain");
    if (terrainIt != obj.end() && terrainIt->second.isObject()) {
        const auto& t = terrainIt->second.asObject();
        const auto bhIt = t.find("baseHeight");
        if (bhIt != t.end() && bhIt->second.isNumber()) {
            def.terrain.baseHeight = static_cast<float>(bhIt->second.asNumber());
        }
        const auto hvIt = t.find("heightVariation");
        if (hvIt != t.end() && hvIt->second.isNumber()) {
            def.terrain.heightVariation = static_cast<float>(hvIt->second.asNumber());
        }
    }

    const auto surfaceIt = obj.find("surface");
    if (surfaceIt != obj.end() && surfaceIt->second.isObject()) {
        const auto& s = surfaceIt->second.asObject();
        const auto readStr = [&s](const std::string& key, std::string& target) {
            const auto it = s.find(key);
            if (it != s.end() && it->second.isString()) {
                target = it->second.asString();
            }
        };
        readStr("top",    def.surface.top);
        readStr("middle", def.surface.middle);
        readStr("base",   def.surface.base);
        const auto mdIt = s.find("middleDepth");
        if (mdIt != s.end() && mdIt->second.isNumber()) {
            def.surface.middleDepth = static_cast<int>(mdIt->second.asNumber());
        }
    }

    const auto atmIt = obj.find("atmosphere");
    if (atmIt != obj.end() && atmIt->second.isObject()) {
        const auto& a = atmIt->second.asObject();
        const auto readColor = [&a](const std::string& key, std::array<float, 3>& target) {
            const auto it = a.find(key);
            if (it != a.end() && it->second.isArray() && it->second.asArray().size() == 3) {
                const auto& arr = it->second.asArray();
                target = {
                    static_cast<float>(arr[0].asNumber()),
                    static_cast<float>(arr[1].asNumber()),
                    static_cast<float>(arr[2].asNumber())
                };
            }
        };
        readColor("skyColor",   def.atmosphere.skyColor);
        readColor("fogColor",   def.atmosphere.fogColor);
        readColor("waterColor", def.atmosphere.waterColor);
    }

    const auto fertilityIt = obj.find("fertility");
    if (fertilityIt != obj.end() && fertilityIt->second.isObject()) {
        const auto& f = fertilityIt->second.asObject();
        const auto readFloat = [&f](const std::string& key, float& target) {
            const auto it = f.find(key);
            if (it != f.end() && it->second.isNumber()) {
                target = static_cast<float>(it->second.asNumber());
            }
        };
        readFloat("nitrogen",   def.fertility.nitrogen);
        readFloat("phosphorus", def.fertility.phosphorus);
        readFloat("potassium",  def.fertility.potassium);
        readFloat("magnesium",  def.fertility.magnesium);
        readFloat("calcium",    def.fertility.calcium);
        readFloat("sulfur",     def.fertility.sulfur);
    }

    const auto colorsIt = obj.find("colors");
    if (colorsIt != obj.end() && colorsIt->second.isObject()) {
        for (const auto& [key, val] : colorsIt->second.asObject()) {
            if (val.isArray() && val.asArray().size() == 3) {
                const auto& arr = val.asArray();
                def.colors[key] = {
                    static_cast<float>(arr[0].asNumber()),
                    static_cast<float>(arr[1].asNumber()),
                    static_cast<float>(arr[2].asNumber())
                };
            }
        }
    }

    return def;
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

// Returns all state combinations for a block state definition.
// The first entry is always the default state (all properties at their default value).
std::vector<std::unordered_map<std::string, BlockProperty>> enumerateStates(
    const BlockStateDefinition& stateDef
) {
    std::vector<std::unordered_map<std::string, BlockProperty>> result;
    result.push_back({});

    for (const auto& [propName, propDef] : stateDef.props) {
        std::vector<std::unordered_map<std::string, BlockProperty>> expanded;
        expanded.reserve(result.size() * propDef.values.size());
        for (const auto& existing : result) {
            for (const auto& val : propDef.values) {
                auto copy = existing;
                copy[propName] = val;
                expanded.push_back(std::move(copy));
            }
        }
        result = std::move(expanded);
    }

    return result;
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
    loadDefinitions<BlockStateDefinition>(root / "states" / "blocks", data.blockStates, parseBlockStateDefinition);
    loadDefinitions<BiomeDefinition>(root / "biomes", data.biomes, parseBiomeDefinition);

    // Assign state ID ranges — preferred blocks get lower IDs
    std::map<std::string, std::uint16_t> defaultStateIds;
    std::uint16_t nextId = 1;

    const auto assignBlock = [&](const std::string& blockId) {
        if (defaultStateIds.contains(blockId)) {
            return;
        }
        std::size_t count = 1;
        const auto stateIt = data.blockStates.find(blockId);
        if (stateIt != data.blockStates.end()) {
            for (const auto& [_, propDef] : stateIt->second.props) {
                count *= propDef.values.size();
            }
        }
        if (static_cast<std::size_t>(nextId) + count > 65535) {
            throw std::runtime_error("Ran out of state IDs assigning block: " + blockId);
        }
        defaultStateIds[blockId] = nextId;
        nextId += static_cast<std::uint16_t>(count);
    };

    for (const char* preferredId : kPreferredBlockOrder) {
        if (data.blocks.contains(preferredId)) {
            assignBlock(preferredId);
        }
    }
    for (const auto& [blockId, _] : data.blocks) {
        assignBlock(blockId);
    }

    // Populate block definitions and state registry
    for (auto& [blockId, block] : data.blocks) {
        block.runtimeId = defaultStateIds.at(blockId);

        const auto stateIt = data.blockStates.find(blockId);
        if (stateIt != data.blockStates.end()) {
            const auto states = enumerateStates(stateIt->second);
            for (std::size_t i = 0; i < states.size(); ++i) {
                const std::uint16_t stateId = block.runtimeId + static_cast<std::uint16_t>(i);
                data.blockIdByStateId[stateId] = blockId;
                data.stateValuesById[stateId]  = states[i];
                const auto variantIt = stateIt->second.variants.find(canonicalStateKey(states[i]));
                if (variantIt != stateIt->second.variants.end() && variantIt->second.modelPath.has_value()) {
                    data.stateModelPathById[stateId] = *variantIt->second.modelPath;
                }
                data.solidByRuntimeId[stateId] = block.solid;
                data.liquidByRuntimeId[stateId] = (block.material == "liquid");
            }
        } else {
            data.blockIdByStateId[block.runtimeId] = blockId;
            data.solidByRuntimeId[block.runtimeId] = block.solid;
            data.liquidByRuntimeId[block.runtimeId] = (block.material == "liquid");
        }
    }

    return data;
}

std::uint16_t runtimeIdForBlock(const GameData& gameData, const std::string& blockId) {
    const auto it = gameData.blocks.find(blockId);
    return it != gameData.blocks.end() ? it->second.runtimeId : 0;
}

const BlockDefinition* findBlockDefinitionForBlockType(const GameData& gameData, const std::uint16_t stateId) {
    const auto idIterator = gameData.blockIdByStateId.find(stateId);
    if (idIterator == gameData.blockIdByStateId.end()) {
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

std::optional<std::uint16_t> blockTypeForItemId(const GameData& gameData, const std::string& itemId) {
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

std::optional<std::uint16_t> runtimeIdForBlockState(
    const GameData& gameData,
    const std::string& blockId,
    const std::unordered_map<std::string, BlockProperty>& properties
) {
    const BlockDefinition* block = findBlockDefinition(gameData, blockId);
    if (block == nullptr) {
        return std::nullopt;
    }

    std::size_t stateCount = 1;
    if (const auto stateIt = gameData.blockStates.find(blockId); stateIt != gameData.blockStates.end()) {
        for (const auto& [_, propDef] : stateIt->second.props) {
            stateCount *= propDef.values.size();
        }
    }

    for (std::size_t i = 0; i < stateCount; ++i) {
        const std::uint16_t stateId = block->runtimeId + static_cast<std::uint16_t>(i);
        const auto valuesIt = gameData.stateValuesById.find(stateId);
        if (valuesIt == gameData.stateValuesById.end()) {
            if (properties.empty()) {
                return stateId;
            }
            continue;
        }

        bool matches = true;
        for (const auto& [key, value] : properties) {
            const auto statePropIt = valuesIt->second.find(key);
            if (statePropIt == valuesIt->second.end() || statePropIt->second != value) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return stateId;
        }
    }

    return std::nullopt;
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

bool isLiquidBlockType(const GameData& gameData, const std::uint16_t stateId) {
    return stateId != 0 && gameData.liquidByRuntimeId[stateId];
}

std::optional<BlockProperty> getBlockProperty(const BlockDefinition& def, const std::string& key) {
    const auto it = def.properties.find(key);
    if (it == def.properties.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<BlockProperty> getStateProperty(const GameData& gameData, const std::uint16_t stateId, const std::string& key) {
    const auto stateIt = gameData.stateValuesById.find(stateId);
    if (stateIt == gameData.stateValuesById.end()) {
        return std::nullopt;
    }
    const auto propIt = stateIt->second.find(key);
    if (propIt == stateIt->second.end()) {
        return std::nullopt;
    }
    return propIt->second;
}

const std::string* modelPathForState(const GameData& gameData, const std::uint16_t stateId) {
    const auto it = gameData.stateModelPathById.find(stateId);
    return it == gameData.stateModelPathById.end() ? nullptr : &it->second;
}

const std::vector<CollisionBox>* collisionBoxesForState(const GameData& gameData, const std::uint16_t stateId) {
    const auto it = gameData.stateCollisionBoxesById.find(stateId);
    return it == gameData.stateCollisionBoxesById.end() ? nullptr : &it->second;
}

}  // namespace voxel
