#include "ui/GameUI.hpp"

#include <format>
#include <iostream>

#include <GLFW/glfw3.h>
#include <RmlUi/Core.h>
#include "RmlUi_Platform_GLFW.h"
#include "RmlUi_Renderer_GL2.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl2.h>

namespace voxel {

// ── Constructor ───────────────────────────────────────────────────────────────

GameUI::GameUI(GLFWwindow* window, int width, int height, const std::string& assetsRoot)
    : window_(window), assetsRoot_(assetsRoot), width_(width), height_(height)
{
    // ── RmlUI ──────────────────────────────────────────────────────────────────
    rmlSystem_.SetWindow(window_);
    Rml::SetSystemInterface(&rmlSystem_);
    Rml::SetRenderInterface(&rmlRenderer_);
    Rml::Initialise();

    rmlRenderer_.SetViewport(width_, height_);

    const std::string fontPath = assetsRoot_ + "/ui/fonts/LatoLatin-Regular.ttf";
    if (!Rml::LoadFontFace(fontPath))
        std::cerr << "[GameUI] Warning: failed to load font: " << fontPath << '\n';

    float dpRatio = 1.0f;
    glfwGetWindowContentScale(window_, &dpRatio, nullptr);

    // Context dimensions in framebuffer (physical) pixels; DPR scales dp→px correctly
    rmlContext_ = Rml::CreateContext("main", Rml::Vector2i(width_, height_));
    if (!rmlContext_) {
        std::cerr << "[GameUI] Failed to create RmlUI context\n";
    } else {
        const std::string hudPath = assetsRoot_ + "/ui/hud.rml";
        hudDoc_ = rmlContext_->LoadDocument(hudPath);
        if (!hudDoc_)
            std::cerr << "[GameUI] Failed to load: " << hudPath << '\n';
        else
            rmlContext_->SetDensityIndependentPixelRatio(dpRatio);  // 1dp = dpRatio fb-pixels
        hudDoc_->Show();
    }

    // ── ImGui ──────────────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;  // Don't persist layout
    ImGui_ImplGlfw_InitForOpenGL(window_, false);  // false = don't install callbacks
    ImGui_ImplOpenGL2_Init();
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowBorderSize = 0.0f;
    ImGui::GetStyle().WindowRounding   = 5.0f;
}

// ── Destructor ────────────────────────────────────────────────────────────────

GameUI::~GameUI() {
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (rmlContext_) Rml::RemoveContext("main");
    Rml::Shutdown();
}

// ── update ────────────────────────────────────────────────────────────────────

void GameUI::update() {
    if (rmlContext_) rmlContext_->Update();

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const auto& d = debugData_;
    ImGui::SetNextWindowPos({10.0f, 10.0f});
    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGui::Begin("##debug", nullptr,
        ImGuiWindowFlags_NoDecoration  |
        ImGuiWindowFlags_NoMove        |
        ImGuiWindowFlags_NoNav         |
        ImGuiWindowFlags_NoInputs      |
        ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("POS    %.1f  %.1f  %.1f",
        d.playerPosition.x, d.playerPosition.y, d.playerPosition.z);
    if (d.targetedBlock.has_value())
        ImGui::Text("TARGET %d  %d  %d",
            d.targetedBlock->x, d.targetedBlock->y, d.targetedBlock->z);
    else
        ImGui::Text("TARGET -");
    ImGui::Separator();
    ImGui::Text("FPS    %d  (%.2f ms)", d.fps, d.frameTimeMs);
    ImGui::Text("CHUNK  %d  %d  %d", d.chunkX, d.chunkY, d.chunkZ);
    ImGui::Text("CHUNKS %d", d.loadedChunks);
    ImGui::Text("SOLID  %d", d.solidBlocks);
    ImGui::Text("FACES  %d", d.visibleFaces);
    ImGui::Text("TRIS   %d", d.triangleCount);
    ImGui::Separator();
    ImGui::Text("BIOME  %s", d.biomeName.empty() ? "-" : d.biomeName.c_str());
    ImGui::Text("TEMP   %.2f", d.temperature);
    ImGui::Text("HUM    %.2f", d.humidity);
    ImGui::Text("RAIN   %.2f", d.rainfall);
    ImGui::Text("ELEV   %.2f", d.elevation);
    ImGui::Text("DRAIN  %.2f", d.drainage);
    ImGui::Text("WTBL   %.2f", d.waterTable);

    ImGui::End();
}

// ── render ────────────────────────────────────────────────────────────────────

void GameUI::render() {
    if (rmlContext_) {
        rmlRenderer_.BeginFrame();
        rmlContext_->Render();
        rmlRenderer_.EndFrame();
    }

    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

// ── GLFW callbacks ────────────────────────────────────────────────────────────

void GameUI::onFramebufferSize(int width, int height) {
    width_  = width;
    height_ = height;
    rmlRenderer_.SetViewport(width, height);
    if (rmlContext_)
        rmlContext_->SetDimensions(Rml::Vector2i(width, height));
}

void GameUI::onContentScale(float /*xscale*/, float /*yscale*/) {}

void GameUI::onKey(int key, int /*scancode*/, int action, int mods) {
    if (rmlContext_)
        RmlGLFW::ProcessKeyCallback(rmlContext_, key, action, mods);
}

void GameUI::onChar(unsigned int codepoint) {
    if (rmlContext_)
        RmlGLFW::ProcessCharCallback(rmlContext_, codepoint);
}

void GameUI::onCursorPos(double x, double y) {
    if (rmlContext_)
        RmlGLFW::ProcessCursorPosCallback(rmlContext_, window_, x, y, 0);
}

void GameUI::onMouseButton(int button, int action, int mods) {
    if (rmlContext_)
        RmlGLFW::ProcessMouseButtonCallback(rmlContext_, button, action, mods);
}

void GameUI::onScroll(double /*xoffset*/, double yoffset) {
    if (rmlContext_)
        RmlGLFW::ProcessScrollCallback(rmlContext_, yoffset, 0);
}

// ── HUD helpers ───────────────────────────────────────────────────────────────

void GameUI::setDebugData(const DebugOverlayData& data) {
    debugData_ = data;
}

void GameUI::setInventory(const Inventory& inventory) {
    if (!hudDoc_) return;

    for (int i = 0; i < kInventorySlots; ++i) {
        const std::string bgId = "slot-" + std::to_string(i) + "-bg";
        if (auto* el = hudDoc_->GetElementById(bgId))
            el->SetClass("selected", i == inventory.selectedIndex);

        const std::string countId = "slot-" + std::to_string(i) + "-count";
        if (auto* el = hudDoc_->GetElementById(countId)) {
            const auto& slot = inventory.slots[static_cast<std::size_t>(i)];
            el->SetInnerRML(slot.count > 0 ? std::to_string(slot.count) : "");
        }
    }
}

void GameUI::setElementText(const char* id, const std::string& text) {
    if (!hudDoc_) return;
    if (auto* el = hudDoc_->GetElementById(id))
        el->SetInnerRML(text);
}

void GameUI::setElementClass(const char* id, const char* cls, bool enable) {
    if (!hudDoc_) return;
    if (auto* el = hudDoc_->GetElementById(id))
        el->SetClass(cls, enable);
}

}  // namespace voxel
