#include "Player.hpp"

#include <algorithm>

namespace voxel {
namespace {
constexpr float kGravity = 24.0f;
constexpr float kJumpVelocity = 8.5f;
constexpr float kMoveSpeed = 5.5f;
constexpr float kMouseSensitivity = 0.09f;

bool collidesAt(const World& world, const GameData& gameData, const Vec3& position) {
    const float minX = position.x - kPlayerRadius;
    const float maxX = position.x + kPlayerRadius;
    const float minY = position.y;
    const float maxY = position.y + kPlayerHeight;
    const float minZ = position.z - kPlayerRadius;
    const float maxZ = position.z + kPlayerRadius;

    const int x0 = static_cast<int>(std::floor(minX));
    const int x1 = static_cast<int>(std::floor(maxX));
    const int y0 = static_cast<int>(std::floor(minY));
    const int y1 = static_cast<int>(std::floor(maxY));
    const int z0 = static_cast<int>(std::floor(minZ));
    const int z1 = static_cast<int>(std::floor(maxZ));

    for (int x = x0; x <= x1; ++x) {
        for (int y = y0; y <= y1; ++y) {
            for (int z = z0; z <= z1; ++z) {
                if (isSolid(world, gameData, x, y, z)) {
                    return true;
                }
            }
        }
    }

    return false;
}

void movePlayerAxis(const World& world, const GameData& gameData, Player& player, const float dx, const float dy, const float dz) {
    if (dx != 0.0f) {
        Vec3 next = player.position;
        next.x += dx;
        if (!collidesAt(world, gameData, next)) {
            player.position.x = next.x;
        } else {
            player.velocity.x = 0.0f;
        }
    }

    if (dz != 0.0f) {
        Vec3 next = player.position;
        next.z += dz;
        if (!collidesAt(world, gameData, next)) {
            player.position.z = next.z;
        } else {
            player.velocity.z = 0.0f;
        }
    }

    if (dy != 0.0f) {
        Vec3 next = player.position;
        next.y += dy;
        if (!collidesAt(world, gameData, next)) {
            player.position.y = next.y;
            player.grounded = false;
        } else {
            if (dy < 0.0f) {
                player.grounded = true;
            }
            player.velocity.y = 0.0f;
        }
    }
}
}  // namespace

Vec3 getLookDirection(const Player& player) {
    const float yawRad = toRadians(player.yaw);
    const float pitchRad = toRadians(player.pitch);
    return normalize({
        std::sin(yawRad) * std::cos(pitchRad),
        std::sin(pitchRad),
        -std::cos(yawRad) * std::cos(pitchRad)
    });
}

Vec3 getEyePosition(const Player& player) {
    return {player.position.x, player.position.y + kEyeHeight, player.position.z};
}

void updateMouseLook(GLFWwindow* window, Player& player) {
    static bool firstMouse = true;
    static double lastX = 0.0;
    static double lastY = 0.0;

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos(window, &cursorX, &cursorY);

    if (firstMouse) {
        lastX = cursorX;
        lastY = cursorY;
        firstMouse = false;
    }

    const double deltaX = cursorX - lastX;
    const double deltaY = cursorY - lastY;
    lastX = cursorX;
    lastY = cursorY;

    player.yaw += static_cast<float>(deltaX) * kMouseSensitivity;
    player.pitch = std::clamp(player.pitch - static_cast<float>(deltaY) * kMouseSensitivity, -89.0f, 89.0f);
}

void updateInput(GLFWwindow* window, InputState& input) {
    const bool placeDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool breakDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool jumpDown = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;

    input.placePressed = placeDown && !input.placeHeld;
    input.breakPressed = breakDown && !input.breakHeld;
    input.jumpPressed = jumpDown && !input.jumpHeld;

    input.placeHeld = placeDown;
    input.breakHeld = breakDown;
    input.jumpHeld = jumpDown;
}

void jump(Player& player, const InputState& input) {
    if (input.jumpPressed && player.grounded) {
        player.velocity.y = kJumpVelocity;
        player.grounded = false;
    }
}

void updateMovement(GLFWwindow* window, const World& world, const GameData& gameData, Player& player, const float deltaTime) {
    const float yawRad = toRadians(player.yaw);
    const Vec3 forward {std::sin(yawRad), 0.0f, -std::cos(yawRad)};
    const Vec3 right {std::cos(yawRad), 0.0f, std::sin(yawRad)};

    Vec3 moveIntent {0.0f, 0.0f, 0.0f};
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        moveIntent = moveIntent + forward;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        moveIntent = moveIntent - forward;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        moveIntent = moveIntent - right;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        moveIntent = moveIntent + right;
    }

    moveIntent = normalize(moveIntent);
    player.velocity.x = moveIntent.x * kMoveSpeed;
    player.velocity.z = moveIntent.z * kMoveSpeed;
    player.velocity.y -= kGravity * deltaTime;

    movePlayerAxis(world, gameData, player, player.velocity.x * deltaTime, 0.0f, 0.0f);
    movePlayerAxis(world, gameData, player, 0.0f, 0.0f, player.velocity.z * deltaTime);
    movePlayerAxis(world, gameData, player, 0.0f, player.velocity.y * deltaTime, 0.0f);

    if (player.position.y < -10.0f) {
        player.position = {0.5f, 80.0f, 0.5f};
        player.velocity = {0.0f, 0.0f, 0.0f};
        player.yaw = 0.0f;
        player.pitch = -20.0f;
    }
}

bool blockWouldIntersectPlayer(const Int3& block, const Player& player) {
    const float blockMinX = static_cast<float>(block.x);
    const float blockMaxX = blockMinX + 1.0f;
    const float blockMinY = static_cast<float>(block.y);
    const float blockMaxY = blockMinY + 1.0f;
    const float blockMinZ = static_cast<float>(block.z);
    const float blockMaxZ = blockMinZ + 1.0f;

    const float playerMinX = player.position.x - kPlayerRadius;
    const float playerMaxX = player.position.x + kPlayerRadius;
    const float playerMinY = player.position.y;
    const float playerMaxY = player.position.y + kPlayerHeight;
    const float playerMinZ = player.position.z - kPlayerRadius;
    const float playerMaxZ = player.position.z + kPlayerRadius;

    const bool overlapX = playerMinX < blockMaxX && playerMaxX > blockMinX;
    const bool overlapY = playerMinY < blockMaxY && playerMaxY > blockMinY;
    const bool overlapZ = playerMinZ < blockMaxZ && playerMaxZ > blockMinZ;
    return overlapX && overlapY && overlapZ;
}
}  // namespace voxel
