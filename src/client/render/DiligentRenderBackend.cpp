#include "render/DiligentRenderBackend.hpp"

#include <stdexcept>
#include <vector>

#include "render/Mesh.hpp"

#if TERRALITE_ENABLE_DILIGENT
#    include <unordered_map>

#    include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#    include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#    include "Graphics/GraphicsEngine/interface/SwapChain.h"
#    include "Common/interface/RefCntAutoPtr.hpp"
#endif

#if TERRALITE_ENABLE_DILIGENT && defined(_WIN32)
#    define GLFW_EXPOSE_NATIVE_WIN32
#    include <GLFW/glfw3.h>
#    include <GLFW/glfw3native.h>
#    include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#    include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"
#    include "Platforms/interface/NativeWindow.h"
#endif

namespace terralite {

struct DiligentRenderBackend::Impl {
#if TERRALITE_ENABLE_DILIGENT
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> swapChain;
    std::unordered_map<std::uintptr_t, Diligent::RefCntAutoPtr<Diligent::IBuffer>> buffers;
    std::unordered_map<std::uintptr_t, Diligent::RefCntAutoPtr<Diligent::ITexture>> textures;
    std::uintptr_t nextResourceId = 1;
    int width = 0;
    int height = 0;
#endif
};

DiligentRenderBackend::DiligentRenderBackend() : impl_(new Impl()) {}

DiligentRenderBackend::~DiligentRenderBackend() {
    delete impl_;
}

const char* DiligentRenderBackend::name() const {
#if TERRALITE_ENABLE_DILIGENT && defined(_WIN32)
    return "Diligent D3D11";
#elif TERRALITE_ENABLE_DILIGENT
    return "Diligent";
#else
    return "Diligent unavailable";
#endif
}

bool DiligentRenderBackend::available() const {
#if TERRALITE_ENABLE_DILIGENT && defined(_WIN32)
    return true;
#else
    return false;
#endif
}

void DiligentRenderBackend::initialize(GLFWwindow* window, const int width, const int height) {
#if TERRALITE_ENABLE_DILIGENT && defined(_WIN32)
    if (window == nullptr) {
        throw std::runtime_error("Cannot initialize Diligent without a GLFW window.");
    }

#    if ENGINE_DLL
    auto getEngineFactoryD3D11 = Diligent::LoadGraphicsEngineD3D11();
    if (getEngineFactoryD3D11 == nullptr) {
        throw std::runtime_error("Failed to load the Diligent D3D11 engine factory.");
    }
    auto* factory = getEngineFactoryD3D11();
#    else
    auto* factory = Diligent::GetEngineFactoryD3D11();
#    endif
    if (factory == nullptr) {
        throw std::runtime_error("Diligent D3D11 engine factory is unavailable.");
    }

    Diligent::EngineD3D11CreateInfo engineCreateInfo;
    engineCreateInfo.GraphicsAPIVersion = Diligent::Version{11, 0};

    Diligent::IDeviceContext* contexts[] = {nullptr};
    factory->CreateDeviceAndContextsD3D11(
        engineCreateInfo,
        impl_->device.RawDblPtr(),
        contexts
    );
    impl_->context = contexts[0];

    Diligent::SwapChainDesc swapChainDesc;
    swapChainDesc.Width = static_cast<Diligent::Uint32>(width);
    swapChainDesc.Height = static_cast<Diligent::Uint32>(height);
    swapChainDesc.ColorBufferFormat = Diligent::TEX_FORMAT_RGBA8_UNORM;
    swapChainDesc.DepthBufferFormat = Diligent::TEX_FORMAT_D32_FLOAT;

    Diligent::FullScreenModeDesc fullscreenDesc;
    const Diligent::NativeWindow nativeWindow{glfwGetWin32Window(window)};
    factory->CreateSwapChainD3D11(
        impl_->device,
        impl_->context,
        swapChainDesc,
        fullscreenDesc,
        nativeWindow,
        impl_->swapChain.RawDblPtr()
    );

    impl_->width = width;
    impl_->height = height;
#else
    (void)window;
    (void)width;
    (void)height;
    throw std::runtime_error("Diligent proof-of-life backend is only wired for Windows D3D11 right now.");
#endif
}

void DiligentRenderBackend::resize(const int width, const int height) {
#if TERRALITE_ENABLE_DILIGENT
    if (impl_->swapChain == nullptr || width <= 0 || height <= 0) {
        return;
    }
    if (impl_->width == width && impl_->height == height) {
        return;
    }
    impl_->swapChain->Resize(static_cast<Diligent::Uint32>(width), static_cast<Diligent::Uint32>(height));
    impl_->width = width;
    impl_->height = height;
#else
    (void)width;
    (void)height;
#endif
}

void DiligentRenderBackend::clearFrame(const Color& clearColor) {
#if TERRALITE_ENABLE_DILIGENT
    if (impl_->context == nullptr || impl_->swapChain == nullptr) {
        return;
    }

    Diligent::ITextureView* renderTarget = impl_->swapChain->GetCurrentBackBufferRTV();
    impl_->context->SetRenderTargets(
        1,
        &renderTarget,
        nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION
    );
    const float color[] = {clearColor.r, clearColor.g, clearColor.b, 1.0f};
    impl_->context->ClearRenderTarget(
        renderTarget,
        color,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION
    );
#else
    (void)clearColor;
#endif
}

void DiligentRenderBackend::present() {
#if TERRALITE_ENABLE_DILIGENT
    if (impl_->swapChain != nullptr) {
        impl_->swapChain->Present();
    }
#endif
}

RenderBufferHandle DiligentRenderBackend::createVertexBuffer(const std::size_t byteCount, const void* data) {
#if TERRALITE_ENABLE_DILIGENT
    if (impl_->device == nullptr || data == nullptr || byteCount == 0) {
        return {};
    }

    Diligent::BufferDesc desc;
    desc.Name = "TERRALITE Diligent vertex buffer";
    desc.Size = static_cast<Diligent::Uint64>(byteCount);
    desc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
    desc.Usage = Diligent::USAGE_IMMUTABLE;

    Diligent::BufferData initData;
    initData.pData = data;
    initData.DataSize = desc.Size;

    Diligent::RefCntAutoPtr<Diligent::IBuffer> buffer;
    impl_->device->CreateBuffer(desc, &initData, buffer.RawDblPtr());
    if (buffer == nullptr) {
        return {};
    }

    const std::uintptr_t id = impl_->nextResourceId++;
    impl_->buffers.emplace(id, std::move(buffer));
    return RenderBufferHandle::diligent(id);
#else
    (void)byteCount;
    (void)data;
    return {};
#endif
}

void DiligentRenderBackend::destroyBuffer(RenderBufferHandle& buffer) {
#if TERRALITE_ENABLE_DILIGENT
    const std::uintptr_t id = buffer.diligentId();
    if (id != 0) {
        impl_->buffers.erase(id);
        buffer.reset();
    }
#else
    (void)buffer;
#endif
}

RenderTextureHandle DiligentRenderBackend::createTexture2D(
    const int width,
    const int height,
    const int channelCount,
    const unsigned char* pixels
) {
#if TERRALITE_ENABLE_DILIGENT
    if (impl_->device == nullptr || pixels == nullptr || width <= 0 || height <= 0) {
        return {};
    }
    if (channelCount != 3 && channelCount != 4) {
        return {};
    }

    std::vector<unsigned char> rgbaPixels;
    const unsigned char* uploadPixels = pixels;
    if (channelCount == 3) {
        rgbaPixels.reserve(static_cast<std::size_t>(width * height * 4));
        for (int index = 0; index < width * height; ++index) {
            rgbaPixels.push_back(pixels[index * 3 + 0]);
            rgbaPixels.push_back(pixels[index * 3 + 1]);
            rgbaPixels.push_back(pixels[index * 3 + 2]);
            rgbaPixels.push_back(255);
        }
        uploadPixels = rgbaPixels.data();
    }

    Diligent::TextureDesc desc;
    desc.Name = "TERRALITE Diligent texture";
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = static_cast<Diligent::Uint32>(width);
    desc.Height = static_cast<Diligent::Uint32>(height);
    desc.MipLevels = 1;
    desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    desc.Usage = Diligent::USAGE_IMMUTABLE;

    Diligent::TextureSubResData subresource;
    subresource.pData = uploadPixels;
    subresource.Stride = static_cast<Diligent::Uint64>(width * 4);

    Diligent::TextureData initData;
    initData.NumSubresources = 1;
    initData.pSubResources = &subresource;

    Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
    impl_->device->CreateTexture(desc, &initData, texture.RawDblPtr());
    if (texture == nullptr) {
        return {};
    }

    const std::uintptr_t id = impl_->nextResourceId++;
    impl_->textures.emplace(id, std::move(texture));
    return RenderTextureHandle::diligent(id);
#else
    (void)width;
    (void)height;
    (void)channelCount;
    (void)pixels;
    return {};
#endif
}

void DiligentRenderBackend::destroyTexture(RenderTextureHandle& texture) {
#if TERRALITE_ENABLE_DILIGENT
    const std::uintptr_t id = texture.diligentId();
    if (id != 0) {
        impl_->textures.erase(id);
        texture.reset();
    }
#else
    (void)texture;
#endif
}

void DiligentRenderBackend::uploadChunkMesh(ChunkMesh& mesh) {
    for (std::size_t surfaceIndex = 0; surfaceIndex < mesh.surfaces.size(); ++surfaceIndex) {
        uploadChunkMeshSurface(mesh, surfaceIndex);
    }
}

bool DiligentRenderBackend::uploadChunkMeshSurface(ChunkMesh& mesh, const std::size_t surfaceIndex) {
    if (surfaceIndex >= mesh.surfaces.size()) {
        return false;
    }

    MeshSurface& surface = mesh.surfaces[surfaceIndex];
    if (surface.vertices.empty()) {
        return false;
    }

    surface.vertexCount = static_cast<int>(surface.vertices.size());
    surface.vertexBuffer = createVertexBuffer(
        surface.vertices.size() * sizeof(MeshVertex),
        surface.vertices.data()
    );
    if (!surface.vertexBuffer.isValid()) {
        surface.vertexCount = 0;
        return false;
    }

    surface.vertices.clear();
    surface.vertices.shrink_to_fit();
    return true;
}

void DiligentRenderBackend::destroyChunkMesh(ChunkMesh& mesh) {
    for (auto& surface : mesh.surfaces) {
        destroyBuffer(surface.vertexBuffer);
        surface.vertexCount = 0;
    }
}

}  // namespace voxel
