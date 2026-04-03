#include <GLFW/glfw3.h>

#include <algorithm>
#include <filesystem>
#include <iostream>

#include "data/GameData.hpp"
#include "game/Game.hpp"
#include "ui/GameUI.hpp"

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

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    glfwWindowHint(GLFW_RED_BITS,     mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS,   mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS,    mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    GLFWwindow* window = glfwCreateWindow(
        mode->width, mode->height, "Voxel Game", primaryMonitor, nullptr);
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

        int fbWidth = 0, fbHeight = 0;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

        voxel::Game   game(std::move(gameData), assetsRoot.string(), dataRoot.string());
        voxel::GameUI ui(window, fbWidth, fbHeight, assetsRoot.string());

        // Forward GLFW events to RmlUI — use window user pointer so lambdas
        // need no captures (required for GLFW's C-style callback signature).
        glfwSetWindowUserPointer(window, &ui);

        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int width, int height) {
            static_cast<voxel::GameUI*>(glfwGetWindowUserPointer(w))
                ->onFramebufferSize(width, height);
        });
        glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int sc, int action, int mods) {
            static_cast<voxel::GameUI*>(glfwGetWindowUserPointer(w))
                ->onKey(key, sc, action, mods);
        });
        glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int cp) {
            static_cast<voxel::GameUI*>(glfwGetWindowUserPointer(w))->onChar(cp);
        });
        glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y) {
            static_cast<voxel::GameUI*>(glfwGetWindowUserPointer(w))->onCursorPos(x, y);
        });
        glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int btn, int action, int mods) {
            static_cast<voxel::GameUI*>(glfwGetWindowUserPointer(w))
                ->onMouseButton(btn, action, mods);
        });
        glfwSetScrollCallback(window, [](GLFWwindow* w, double xoff, double yoff) {
            static_cast<voxel::GameUI*>(glfwGetWindowUserPointer(w))->onScroll(xoff, yoff);
        });

        float lastTime = static_cast<float>(glfwGetTime());

        while (!glfwWindowShouldClose(window)) {
            const float currentTime = static_cast<float>(glfwGetTime());
            const float deltaTime = std::min(currentTime - lastTime, 0.05f);
            lastTime = currentTime;

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            game.update(window, deltaTime);
            ui.setDebugData(game.getDebugData());
            ui.setInventory(game.getInventory());
            ui.update();

            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
            game.render(fbWidth, fbHeight);
            game.renderHotbarIcons(fbWidth, fbHeight);
            ui.render();  // drawn on top of the 3D scene

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
