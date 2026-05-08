#pragma once

#include <vector>

#include <entt/entt.hpp>

#include "common/ecs/Components.hpp"

namespace voxel::ecs {

void updateMovementSystem(entt::registry& registry, float deltaTime);
void updatePhysicsSystem(entt::registry& registry, float deltaTime);
void updateAISystem(entt::registry& registry, float deltaTime);
void updateLifetimeSystem(entt::registry& registry, float deltaTime);
std::vector<RenderSyncState> collectRenderSyncSystem(const entt::registry& registry);

}  // namespace voxel::ecs
