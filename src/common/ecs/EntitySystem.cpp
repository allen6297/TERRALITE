#include "common/ecs/EntitySystem.hpp"

#include <utility>

#include "common/ecs/Systems.hpp"

namespace voxel::ecs {

entt::entity EntitySystem::spawnEntity(const std::string& name, const TransformComponent& transform) {
    const entt::entity entity = registry_.create();
    registry_.emplace<TransformComponent>(entity, transform);
    registry_.emplace<NameComponent>(entity, NameComponent{name});
    return entity;
}

entt::entity EntitySystem::spawnPrefab(const EntityPrefab& prefab, const Vec3& position) {
    TransformComponent transform = prefab.transform;
    transform.position = position;

    const entt::entity entity = spawnEntity(prefab.displayName.empty() ? prefab.id : prefab.displayName, transform);
    if (!prefab.id.empty()) {
        registry_.emplace<PrefabComponent>(entity, PrefabComponent{prefab.id});
    }
    if (!prefab.modelId.empty()) {
        registry_.emplace<RenderComponent>(entity, RenderComponent{prefab.modelId});
    }
    if (prefab.hasVelocity) {
        registry_.emplace<VelocityComponent>(entity, prefab.velocity);
    }
    if (prefab.hasHealth) {
        registry_.emplace<HealthComponent>(entity, prefab.health);
    }
    if (prefab.hasPhysics) {
        registry_.emplace<PhysicsComponent>(entity, prefab.physics);
    }
    if (prefab.hasScript) {
        registry_.emplace<ScriptComponent>(entity, prefab.script);
    }

    return entity;
}

void EntitySystem::destroy(const entt::entity entity) {
    if (registry_.valid(entity)) {
        registry_.destroy(entity);
    }
}

void EntitySystem::clear() {
    registry_.clear();
}

void EntitySystem::update(const float deltaTime) {
    updateAISystem(registry_, deltaTime);
    updatePhysicsSystem(registry_, deltaTime);
    updateMovementSystem(registry_, deltaTime);
    updateLifetimeSystem(registry_, deltaTime);
}

std::vector<RenderSyncState> EntitySystem::collectRenderSync() const {
    return collectRenderSyncSystem(registry_);
}

std::vector<EntityDebugInfo> EntitySystem::inspectEntities() const {
    std::vector<EntityDebugInfo> entities;
    entities.reserve(registry_.view<const TransformComponent>().size());

    registry_.view<const TransformComponent>().each([this, &entities](const entt::entity entity, const TransformComponent& transform) {
        EntityDebugInfo info;
        info.entity = entity;
        info.position = transform.position;

        if (const auto* name = registry_.try_get<NameComponent>(entity)) {
            info.name = name->name;
        }
        if (const auto* prefab = registry_.try_get<PrefabComponent>(entity)) {
            info.prefabId = prefab->prefabId;
        }

        info.hasVelocity = registry_.all_of<VelocityComponent>(entity);
        info.hasHealth = registry_.all_of<HealthComponent>(entity);
        info.hasInventory = registry_.all_of<InventoryComponent>(entity);
        info.hasRender = registry_.all_of<RenderComponent>(entity);
        info.hasScript = registry_.all_of<ScriptComponent>(entity);
        info.hasAI = registry_.all_of<AIComponent>(entity);

        entities.push_back(std::move(info));
    });

    return entities;
}

std::size_t EntitySystem::livingEntityCount() const {
    return registry_.view<const TransformComponent>().size();
}

}  // namespace voxel::ecs
