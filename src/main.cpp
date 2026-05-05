#include <GLFW/glfw3.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "data/GameData.hpp"
#include "game/Game.hpp"
#include "network/NetworkManager.hpp"
#include "pack/AssetPackManager.hpp"
#include "pack/PackManager.hpp"
#include "pack/ScriptManager.hpp"
#include "ui/GameUI.hpp"

namespace {
constexpr std::uint16_t kDefaultMultiplayerPort = 27015;
std::atomic_bool gServerRunning {true};

void handleServerSignal(int) {
    gServerRunning = false;
}

std::filesystem::path findProjectRoot() {
    std::filesystem::path current = std::filesystem::current_path();

    while (!current.empty()) {
        if (std::filesystem::exists(current / "packs")) {
            return current;
        }

        const auto parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }

    throw std::runtime_error("Could not find project root containing packs/");
}

std::uint16_t parsePort(const char* value) {
    const int port = std::stoi(value);
    if (port <= 0 || port > 65535) {
        throw std::runtime_error("Port must be between 1 and 65535.");
    }
    return static_cast<std::uint16_t>(port);
}

bool isDedicatedServerMode(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--server" || arg == "--dedicated-server") {
            return true;
        }
    }
    return false;
}

int runDedicatedServer(int argc, char** argv) {
    std::uint16_t port = kDefaultMultiplayerPort;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--server" || arg == "--dedicated-server") {
            continue;
        }
        if (arg == "--port") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--port requires a value.");
            }
            port = parsePort(argv[++i]);
            continue;
        }
        throw std::runtime_error("Unknown dedicated server argument: " + arg);
    }

    std::signal(SIGINT, handleServerSignal);
    std::signal(SIGTERM, handleServerSignal);

    voxel::NetworkManager network;
    if (!network.startServer(port)) {
        return 1;
    }

    std::cout << "VoxelGame dedicated server running on port " << port << ". Press Ctrl+C to stop.\n";
    while (gServerRunning) {
        network.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    return 0;
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (isDedicatedServerMode(argc, argv)) {
            return runDedicatedServer(argc, argv);
        }
    } catch (const std::exception& error) {
        std::cerr << "Fatal server error: " << error.what() << '\n';
        return 1;
    }

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    GLFWmonitor*       primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode           = glfwGetVideoMode(primaryMonitor);
    glfwWindowHint(GLFW_RED_BITS,     mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS,   mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS,    mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    GLFWwindow* window = glfwCreateWindow(
        mode->width, mode->height, "Voxel Game", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create the window. Make sure GLFW and an OpenGL driver are installed.\n";
        glfwTerminate();
        return 1;
    }

    int monitorX = 0, monitorY = 0;
    glfwGetMonitorPos(primaryMonitor, &monitorX, &monitorY);
    glfwSetWindowPos(window, monitorX, monitorY);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    try {
        // ── Pack system ───────────────────────────────────────────────────────
        const std::filesystem::path projectRoot = findProjectRoot();
        const std::filesystem::path packsDir    = projectRoot / "packs";

        voxel::PackManager packManager;
        packManager.discover(packsDir);

        const voxel::Pack* basePack = packManager.findPack("base");
        if (!basePack) throw std::runtime_error("Required 'base' pack not found in packs/");

        voxel::AssetPackManager assetManager(packManager);

        // Derive legacy path strings from the base pack for Game and GameUI.
        // These will be replaced with AssetPackManager calls when ScriptManager
        // is integrated and the internals of Game/GameUI are migrated.
        const std::string assetsRoot = (basePack->path() / "assets").string();

        // ── Game data ─────────────────────────────────────────────────────────
        voxel::ScriptManager scriptManager;
        voxel::GameData gameData = scriptManager.loadGameData(
            packManager, projectRoot / "engine" / "scripts");

        // ── Game & UI ─────────────────────────────────────────────────────────
        int fbWidth = 0, fbHeight = 0;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

        voxel::NetworkManager network;
        voxel::NetworkManager* activeNetwork = nullptr;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--host") {
                const std::uint16_t port =
                    (i + 1 < argc && argv[i + 1][0] != '-') ? parsePort(argv[++i]) : kDefaultMultiplayerPort;
                if (network.startServer(port)) {
                    activeNetwork = &network;
                }
            } else if (arg == "--connect") {
                if (i + 1 >= argc) {
                    throw std::runtime_error("--connect requires a host name or IP address.");
                }
                const std::string hostName = argv[++i];
                const std::uint16_t port =
                    (i + 1 < argc && argv[i + 1][0] != '-') ? parsePort(argv[++i]) : kDefaultMultiplayerPort;
                if (network.connectToServer(hostName, port)) {
                    activeNetwork = &network;
                }
            }
        }

        voxel::Game   game(std::move(gameData), assetsRoot, activeNetwork);
        voxel::GameUI ui(window, fbWidth, fbHeight, assetsRoot);

        // Forward GLFW events to RmlUI — use window user pointer so lambdas
        // need no captures (required for GLFW's C-style callback signature).
        glfwSetWindowUserPointer(window, &ui);

        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int width, int height) {
            static_cast<voxel::GameUI*>(glfwGetWindowUserPointer(w))
                ->onFramebufferSize(width, height);
        });
        glfwSetWindowContentScaleCallback(window, [](GLFWwindow* w, float xscale, float yscale) {
            static_cast<voxel::GameUI*>(glfwGetWindowUserPointer(w))
                ->onContentScale(xscale, yscale);
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

        float lastTime   = static_cast<float>(glfwGetTime());
        bool  escWasDown = false;

        while (!glfwWindowShouldClose(window)) {
            const float currentTime = static_cast<float>(glfwGetTime());
            const float deltaTime   = std::min(currentTime - lastTime, 0.05f);
            lastTime = currentTime;

            // ESC edge-triggers cursor capture toggle
            const bool escDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
            if (escDown && !escWasDown) {
                const bool captured =
                    glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
                glfwSetInputMode(window, GLFW_CURSOR,
                    captured ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
                if (!captured && glfwRawMouseMotionSupported())
                    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            }
            escWasDown = escDown;

            game.update(window, deltaTime);
            ui.setDebugData(game.getDebugData());
            ui.setInventory(game.getInventory());
            ui.update();

            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
            game.render(fbWidth, fbHeight);
            game.renderHotbarIcons(fbWidth, fbHeight);
            ui.render();

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
