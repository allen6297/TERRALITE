cmake_minimum_required(VERSION 3.20)
project(VoxelGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH
        "/opt/homebrew"
        "/opt/homebrew/opt/glfw"
        "/usr/local"
        "/usr/local/opt/glfw"
    )
endif()

find_package(glfw3 CONFIG QUIET)

if(TARGET glfw)
    set(GLFW_TARGET glfw)
elseif(TARGET glfw::glfw)
    set(GLFW_TARGET glfw::glfw)
else()
    find_path(GLFW_INCLUDE_DIR GLFW/glfw3.h
        PATHS
            /opt/homebrew/include
            /opt/homebrew/opt/glfw/include
            /usr/local/include
            /usr/local/opt/glfw/include
        REQUIRED
    )
    find_library(GLFW_LIBRARY glfw
        PATHS
            /opt/homebrew/lib
            /opt/homebrew/opt/glfw/lib
            /usr/local/lib
            /usr/local/opt/glfw/lib
        REQUIRED
    )

    add_library(glfw_manual UNKNOWN IMPORTED)
    set_target_properties(glfw_manual PROPERTIES
        IMPORTED_LOCATION "${GLFW_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GLFW_INCLUDE_DIR}"
    )
    set(GLFW_TARGET glfw_manual)
endif()

find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)

add_executable(VoxelGame
    src/data/GameData.cpp
    src/data/JsonValue.cpp
    src/main.cpp
    src/game/Game.cpp
    src/Math.cpp
    src/player/Inventory.cpp
    src/render/Mesh.cpp
    src/Player.cpp
    src/render/Renderer.cpp
    src/render/ShaderProgram.cpp
    src/render/TextureManager.cpp
    src/world/TerrainGenerator.cpp
    src/world/World.cpp
)
target_include_directories(VoxelGame PRIVATE include third_party)
target_compile_features(VoxelGame PRIVATE cxx_std_20)
target_link_libraries(VoxelGame PRIVATE
    ${GLFW_TARGET}
    ${OPENGL_FRAMEWORK}
    ${COCOA_FRAMEWORK}
    ${IOKIT_FRAMEWORK}
    ${COREVIDEO_FRAMEWORK}
)

if(APPLE)
    target_compile_definitions(VoxelGame PRIVATE GL_SILENCE_DEPRECATION)
endif()
