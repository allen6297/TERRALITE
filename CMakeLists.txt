cmake_minimum_required(VERSION 3.20)
project(VoxelGame LANGUAGES CXX)
include(FetchContent)

# Allow older third-party CMakeLists (e.g. miniz) that declare cmake_minimum < 3.5
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH
        "/opt/homebrew"
        "/opt/homebrew/opt/glfw"
        "/opt/homebrew/opt/freetype"
        "/usr/local"
        "/usr/local/opt/glfw"
        "/usr/local/opt/freetype"
    )
endif()

# ── QuickJS-NG (JavaScript engine for ScriptManager) ─────────────────────────
FetchContent_Declare(
    quickjs
    GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
    GIT_TAG        v0.9.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(quickjs)

# ── miniz (zip reading for pack system) ──────────────────────────────────────
FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG        3.0.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(miniz)

# ── FreeType (required by RmlUI font engine) ──────────────────────────────────
find_package(Freetype REQUIRED)

# ── RmlUI ─────────────────────────────────────────────────────────────────────
set(BUILD_SHARED_LIBS          OFF CACHE BOOL "" FORCE)
set(RMLUI_SAMPLES              OFF CACHE BOOL "" FORCE)
set(RMLUI_TESTS                OFF CACHE BOOL "" FORCE)
set(RMLUI_THIRDPARTY_CONTAINERS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    RmlUi
    GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
    GIT_TAG        6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(RmlUi)

# ── Dear ImGui (debug overlay) ────────────────────────────────────────────────
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp
        src/pack/ScriptBindingCustom.cpp
)

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
            "C:/Program Files/GLFW/include"
            "C:/glfw/include"
        REQUIRED
    )
    find_library(GLFW_LIBRARY glfw
        PATHS
            /opt/homebrew/lib
            /opt/homebrew/opt/glfw/lib
            /usr/local/lib
            /usr/local/opt/glfw/lib
            "C:/Program Files/GLFW/lib"
            "C:/glfw/lib"
        REQUIRED
    )

    add_library(glfw_manual UNKNOWN IMPORTED)
    set_target_properties(glfw_manual PROPERTIES
        IMPORTED_LOCATION "${GLFW_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GLFW_INCLUDE_DIR}"
    )
    set(GLFW_TARGET glfw_manual)
endif()

# ── Platform-specific OpenGL and windowing libraries ──────────────────────────
if(APPLE)
    find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
    find_library(COCOA_FRAMEWORK   Cocoa REQUIRED)
    find_library(IOKIT_FRAMEWORK   IOKit REQUIRED)
    find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
    set(PLATFORM_LIBS
        ${OPENGL_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
    )
elseif(WIN32)
    set(PLATFORM_LIBS opengl32)
else()  # Linux / BSD
    find_package(OpenGL REQUIRED)
    set(PLATFORM_LIBS OpenGL::GL dl)
endif()

add_executable(VoxelGame
    src/data/GameData.cpp
    src/data/JsonValue.cpp
    src/pack/Pack.cpp
    src/pack/PackManager.cpp
    src/pack/AssetPackManager.cpp
    src/pack/ScriptManager.cpp
    src/pack/ScriptBindingsCustom.cpp
    src/pack/generated/ParseBindings.cpp
    src/main.cpp
    src/game/Game.cpp
    src/game/GameBlocks.cpp
    src/game/GameLifecycle.cpp
    src/game/GameRender.cpp
    src/game/GameUpdate.cpp
    src/Math.cpp
    src/player/Inventory.cpp
    src/render/Mesh.cpp
    src/render/ModelManager.cpp
    src/Player.cpp
    src/render/Renderer.cpp
    src/render/ShaderProgram.cpp
    src/render/TextureManager.cpp
    src/ui/GameUI.cpp
    src/world/BiomeDefinition.cpp
    src/world/TerrainGenerator.cpp
    src/world/World.cpp
    ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_GLFW.cpp
    ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL2.cpp
    ${IMGUI_SOURCES}
        src/pack/ScriptBindingCustom.cpp
)
target_include_directories(VoxelGame PRIVATE
    include
    third_party
    ${quickjs_SOURCE_DIR}
    ${rmlui_SOURCE_DIR}/Include
    ${rmlui_SOURCE_DIR}/Backends
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_features(VoxelGame PRIVATE cxx_std_20)
target_link_libraries(VoxelGame PRIVATE
    miniz
    qjs
    ${GLFW_TARGET}
    ${PLATFORM_LIBS}
    RmlUi::Core
    Freetype::Freetype
)

# ── Code generation ───────────────────────────────────────────────────────────
find_program(NODEJS node)
if(NODEJS)
    add_custom_target(generate_bindings
        COMMAND ${NODEJS} ${CMAKE_SOURCE_DIR}/tools/codegen/generate.js
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        SOURCES
            ${CMAKE_SOURCE_DIR}/tools/codegen/generate.js
            ${CMAKE_SOURCE_DIR}/tools/codegen/schema/block.js
            ${CMAKE_SOURCE_DIR}/tools/codegen/schema/item.js
            ${CMAKE_SOURCE_DIR}/tools/codegen/schema/biome.js
        COMMENT "Regenerating parse bindings and voxel.d.ts from schema (cmake --build . --target generate_bindings)"
    )
else()
    message(STATUS "Node.js not found — using pre-generated bindings (run node tools/codegen/generate.js to regenerate)")
endif()

if(APPLE)
    target_compile_definitions(VoxelGame PRIVATE GL_SILENCE_DEPRECATION)
endif()


# ── Install & CPack (distributable package) ───────────────────────────────────
install(TARGETS VoxelGame
    RUNTIME DESTINATION .   # Windows/Linux: place exe at package root
    BUNDLE  DESTINATION .   # macOS: place .app at package root
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/packs/ DESTINATION packs)

set(CPACK_PACKAGE_NAME        "VoxelGame")
set(CPACK_PACKAGE_VERSION     "0.1.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A voxel survival game")
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "VoxelGame")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
elseif(WIN32)
    # Produces both a ZIP and an NSIS installer (NSIS must be installed)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "VoxelGame")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
