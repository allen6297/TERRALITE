#pragma once

#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

#include <string>

#include "render/TextureManager.hpp"

namespace voxel {
class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    void initialize();
    void useSurface(const TextureResource* albedo, const TextureResource* emissive, const TextureResource& fallbackBlack, float opacity);
    void stop() const;

private:
    GLhandleARB program_ = nullptr;
    int albedoUniform_ = -1;
    int emissiveUniform_ = -1;
    int hasEmissiveUniform_ = -1;
    int opacityUniform_ = -1;
};
}  // namespace voxel
