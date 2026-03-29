#include <GLFW/glfw3.h>

#include <algorithm>
#include <filesystem>
#include <iostream>

#include "data/GameData.hpp"
#include "game/Game.hpp"

namespace {
std::filesystem::path findProjectRoot() {
    std::filesystem::path current = std::filesystem::current_path();

    while (!current.empty()) {
        const auto dataDir = current / "data";
        const auto assetsDir = current / "assets";
        if (std::filesystem::exists(dataDir) && std::filesystem::exists(assetsDir)) {
            return current;
        }

        const auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    throw std::runtime_error("Could not find project root containing data/ and assets/");
}
}  // namespace

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Voxel Game", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create the window. Make sure GLFW is installed with Homebrew.\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    voxel::GameData gameData;
    try {
        const std::filesystem::path projectRoot = findProjectRoot();
        const std::filesystem::path dataRoot = projectRoot / "data";
        const std::filesystem::path assetsRoot = projectRoot / "assets";

        gameData = voxel::loadGameData(dataRoot.string());
        std::cout << "Loaded " << gameData.blocks.size() << " blocks and "
                  << gameData.items.size() << " items.\n";

        voxel::Game game(std::move(gameData), assetsRoot.string());
        float lastTime = static_cast<float>(glfwGetTime());

        while (!glfwWindowShouldClose(window)) {
            const float currentTime = static_cast<float>(glfwGetTime());
            const float deltaTime = std::min(currentTime - lastTime, 0.05f);
            lastTime = currentTime;

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            game.update(window, deltaTime);

            int framebufferWidth = 0;
            int framebufferHeight = 0;
            glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
            game.render(framebufferWidth, framebufferHeight);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    } catch (const std::exception& error) {
        std::cerr << "Failed to load game data: " << error.what() << '\n';
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
