#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "common/Math.hpp"
#include "common/player/Inventory.hpp"

namespace voxel::ecs {

struct TransformComponent {
    Vec3 position {0.0f, 0.0f, 0.0f};
    Vec3 scale {1.0f, 1.0f, 1.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
};

struct VelocityComponent {
    Vec3 velocity {0.0f, 0.0f, 0.0f};
    Vec3 acceleration {0.0f, 0.0f, 0.0f};
};

struct HealthComponent {
    float current = 20.0f;
    float max = 20.0f;
};

struct InventoryComponent {
    Inventory inventory {};
};

struct NameComponent {
    std::string name;
};

struct RenderComponent {
    std::string modelId;
    bool visible = true;
};

struct ScriptComponent {
    std::string scriptId;
    std::vector<std::string> tags;
};

struct LifetimeComponent {
    float remainingSeconds = 0.0f;
};

struct PhysicsComponent {
    bool affectedByGravity = true;
    bool grounded = false;
    float gravity = 24.0f;
};

struct AIComponent {
    std::string behaviorId;
    float tickAccumulator = 0.0f;
};

struct PrefabComponent {
    std::string prefabId;
};

struct RenderSyncState {
    entt::entity entity;
    Vec3 position {0.0f, 0.0f, 0.0f};
    Vec3 scale {1.0f, 1.0f, 1.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
    std::string modelId;
};

}  // namespace voxel::ecs
