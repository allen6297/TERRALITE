#pragma once

#include <cstdint>

namespace voxel {
enum class RenderBackendKind {
    None,
    OpenGL,
    Diligent,
};

struct RenderBufferHandle {
    RenderBackendKind backend = RenderBackendKind::None;
    std::uintptr_t value = 0;

    static RenderBufferHandle openGl(std::uint32_t id) {
        return {RenderBackendKind::OpenGL, id};
    }

    static RenderBufferHandle diligent(std::uintptr_t id) {
        return {RenderBackendKind::Diligent, id};
    }

    bool isValid() const {
        return value != 0;
    }

    std::uint32_t openGlId() const {
        return backend == RenderBackendKind::OpenGL ? static_cast<std::uint32_t>(value) : 0;
    }

    std::uintptr_t diligentId() const {
        return backend == RenderBackendKind::Diligent ? value : 0;
    }

    void reset() {
        backend = RenderBackendKind::None;
        value = 0;
    }
};

struct RenderTextureHandle {
    RenderBackendKind backend = RenderBackendKind::None;
    std::uintptr_t value = 0;

    static RenderTextureHandle openGl(std::uint32_t id) {
        return {RenderBackendKind::OpenGL, id};
    }

    static RenderTextureHandle diligent(std::uintptr_t id) {
        return {RenderBackendKind::Diligent, id};
    }

    bool isValid() const {
        return value != 0;
    }

    std::uint32_t openGlId() const {
        return backend == RenderBackendKind::OpenGL ? static_cast<std::uint32_t>(value) : 0;
    }

    std::uintptr_t diligentId() const {
        return backend == RenderBackendKind::Diligent ? value : 0;
    }

    void reset() {
        backend = RenderBackendKind::None;
        value = 0;
    }
};

struct RenderViewport {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};
}  // namespace voxel
