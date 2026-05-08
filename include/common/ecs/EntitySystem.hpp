#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "common/ecs/Components.hpp"

namespace voxel::ecs {

struct EntityPrefab {
    std::string id;
    std::string displayName;
    std::string modelId;
    TransformComponent transform {};
    VelocityComponent velocity {};
    HealthComponent health {};
    PhysicsComponent physics {};
    ScriptComponent script {};
    bool hasVelocity = false;
    bool hasHealth = false;
    bool hasPhysics = false;
    bool hasScript = false;
};

struct EntityDebugInfo {
    entt::entity entity = entt::null;
    std::string name;
    std::string prefabId;
    Vec3 position {0.0f, 0.0f, 0.0f};
    bool hasVelocity = false;
    bool hasHealth = false;
    bool hasInventory = false;
    bool hasRender = false;
    bool hasScript = false;
    bool hasAI = false;
};

class EntitySystem {
public:
    EntitySystem() = default;

    entt::registry& registry() { return registry_; }
    const entt::registry& registry() const { return registry_; }

    entt::entity spawnEntity(const std::string& name, const TransformComponent& transform = {});
    entt::entity spawnPrefab(const EntityPrefab& prefab, const Vec3& position);
    void destroy(entt::entity entity);
    void clear();

    void update(float deltaTime);
    std::vector<RenderSyncState> collectRenderSync() const;
    std::vector<EntityDebugInfo> inspectEntities() const;
    std::size_t livingEntityCount() const;

private:
    entt::registry registry_;
};

}  // namespace voxel::ecs
