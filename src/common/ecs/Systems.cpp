#include "common/ecs/Systems.hpp"

namespace voxel::ecs {

void updateMovementSystem(entt::registry& registry, const float deltaTime) {
    auto view = registry.view<TransformComponent, VelocityComponent>();
    for (auto [entity, transform, velocity] : view.each()) {
        (void)entity;
        transform.position = transform.position + velocity.velocity * deltaTime;
    }
}

void updatePhysicsSystem(entt::registry& registry, const float deltaTime) {
    auto view = registry.view<VelocityComponent, PhysicsComponent>();
    for (auto [entity, velocity, physics] : view.each()) {
        (void)entity;
        if (!physics.affectedByGravity || physics.grounded) {
            continue;
        }
        velocity.velocity.y -= physics.gravity * deltaTime;
    }
}

void updateAISystem(entt::registry& registry, const float deltaTime) {
    auto view = registry.view<AIComponent>();
    for (auto [entity, ai] : view.each()) {
        (void)entity;
        ai.tickAccumulator += deltaTime;
    }
}

void updateLifetimeSystem(entt::registry& registry, const float deltaTime) {
    std::vector<entt::entity> expired;
    auto view = registry.view<LifetimeComponent>();
    for (auto [entity, lifetime] : view.each()) {
        lifetime.remainingSeconds -= deltaTime;
        if (lifetime.remainingSeconds <= 0.0f) {
            expired.push_back(entity);
        }
    }

    for (const entt::entity entity : expired) {
        if (registry.valid(entity)) {
            registry.destroy(entity);
        }
    }
}

std::vector<RenderSyncState> collectRenderSyncSystem(const entt::registry& registry) {
    std::vector<RenderSyncState> states;
    auto view = registry.view<const TransformComponent, const RenderComponent>();
    states.reserve(view.size_hint());

    for (auto [entity, transform, render] : view.each()) {
        if (!render.visible) {
            continue;
        }
        states.push_back({
            entity,
            transform.position,
            transform.scale,
            transform.yaw,
            transform.pitch,
            transform.roll,
            render.modelId,
        });
    }

    return states;
}

}  // namespace voxel::ecs
