# Diligent Engine Integration

TERRALITE keeps game and pack rendering code behind `IRenderBackend`. The current shipping backend is `OpenGLRenderBackend`; Diligent integration is experimental and guarded by CMake.

## CMake Switch

Configure with:

```powershell
cmake -S . -B cmake-build-diligent -DTERRALITE_ENABLE_DILIGENT=ON
```

When enabled, CMake fetches `DiligentCore`.

The source directory intentionally uses that exact name because Diligent headers expect the module layout. `DiligentTools` and `DiligentFX` are intentionally deferred: the first TERRALITE spike only needs Core/device/backend APIs, and Tools currently requires a Python environment with `pip` for `libclang`.

## Current Status

- The dependency hook is present, verified, and off by default.
- Normal OpenGL builds do not fetch or link Diligent.
- `voxel_client_support` receives `TERRALITE_ENABLE_DILIGENT=1` only when the option is enabled.
- Windows links D3D11, D3D12, OpenGL, and Vulkan Diligent backends; non-Windows links OpenGL and Vulkan.
- The Diligent-enabled configure currently needs an explicit Python interpreter when Python is not on `PATH`.
- A Windows Debug DiligentCore-only build was verified with `TERRALITE_ENABLE_DILIGENT=ON`; `Terralite.exe` builds and all three test executables pass under that build tree.
- `DiligentRenderBackend` now has a Windows D3D11 proof-of-life path that creates a Diligent device, immediate context, swap chain, clears the back buffer, and presents.
- Run the proof path with `Terralite.exe --diligent-proof`; for automated smoke checks, use `Terralite.exe --diligent-proof-frames 1`.
- The renderer-facing boundary is already in place for frame setup, mesh buffers, texture handles, preview viewports, and high-level draw calls.
- Mesh vertex buffers and textures now use backend-tagged opaque handles (`RenderBufferHandle` / `RenderTextureHandle`) instead of exposing OpenGL object names directly to shared mesh and texture resource structs.
- The Diligent proof backend owns Diligent `IBuffer` and `ITexture` resources behind those handles. The smoke path creates and destroys one vertex buffer and one texture before clearing/presenting.
- The Diligent proof backend now supports the chunk mesh upload contract (`uploadChunkMeshSurface`, `uploadChunkMesh`, and `destroyChunkMesh`). The smoke path uploads a tiny `ChunkMesh` surface through that path.

## Next Spike

Begin a minimal Diligent chunk draw path while keeping the existing OpenGL backend as the default.
