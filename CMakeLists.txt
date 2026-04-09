cmake_minimum_required(VERSION 3.20)
project(VoxelGame LANGUAGES CXX)
include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH
        "/opt/homebrew"
        "/opt/homebrew/opt/glfw"
        "/opt/homebrew/opt/freetype"
        "/opt/homebrew/opt/qt"
        "/opt/homebrew/opt/qt/lib/cmake"
        "/opt/homebrew/opt/qt@6"
        "/opt/homebrew/opt/qt@6/lib/cmake"
        "/usr/local"
        "/usr/local/opt/glfw"
        "/usr/local/opt/freetype"
        "/usr/local/opt/qt"
        "/usr/local/opt/qt/lib/cmake"
        "/usr/local/opt/qt@6"
        "/usr/local/opt/qt@6/lib/cmake"
    )
endif()

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
    # RmlUI GL2 + GLFW backends (local copies with macOS fixes)
    src/ui/RmlUi_Renderer_GL2.cpp
    src/ui/RmlUi_Platform_GLFW.cpp
)
target_include_directories(VoxelGame PRIVATE
    include
    third_party
    ${rmlui_SOURCE_DIR}/Include
    ${rmlui_SOURCE_DIR}/Backends
)
target_compile_features(VoxelGame PRIVATE cxx_std_20)
target_link_libraries(VoxelGame PRIVATE
    ${GLFW_TARGET}
    ${OPENGL_FRAMEWORK}
    ${COCOA_FRAMEWORK}
    ${IOKIT_FRAMEWORK}
    ${COREVIDEO_FRAMEWORK}
    RmlUi::Core
)

# Copy the bundled Lato font from RmlUI samples so there's no manual setup
add_custom_command(TARGET VoxelGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_SOURCE_DIR}/assets/ui/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${rmlui_SOURCE_DIR}/Samples/assets/LatoLatin-Regular.ttf"
        "${CMAKE_SOURCE_DIR}/assets/ui/fonts/LatoLatin-Regular.ttf"
    COMMENT "Copying RmlUI sample font to assets/ui/fonts/"
)

if(APPLE)
    target_compile_definitions(VoxelGame PRIVATE GL_SILENCE_DEPRECATION)
endif()

option(BUILD_CONTENT_EDITOR "Build the standalone Qt content editor if Qt6 Widgets/WebEngine are available" ON)

if(BUILD_CONTENT_EDITOR)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    find_package(Qt6 COMPONENTS Widgets WebEngineWidgets WebChannel QUIET)
    if(Qt6_FOUND)
        add_executable(ContentEditor
            tools/content_editor/main.cpp
            tools/content_editor/MainWindow.cpp
            src/data/JsonValue.cpp
        )
        target_include_directories(ContentEditor PRIVATE
            include
            tools/content_editor
        )
        target_link_libraries(ContentEditor PRIVATE
            Qt6::Widgets
            Qt6::WebEngineWidgets
            Qt6::WebChannel
        )
        target_compile_features(ContentEditor PRIVATE cxx_std_20)
    else()
        message(STATUS "Qt6 Widgets/WebEngine not found; skipping ContentEditor target")
    endif()
endif()
