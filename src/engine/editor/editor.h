#pragma once

#include "renderer/render_types.h"

#include <cstddef>
#include <cstdint>
#include <string>

class MetalRenderer;
class Scene;
class GameObject;

class Editor
{
public:
    Editor();
    ~Editor();

    bool init(void* nativeView, void* device);
    void shutdown();

    void beginFrame(void* nativeView);
    void endFrame();

    // Call inside the Metal UI render pass.
    void render(void* commandBuffer, void* renderEncoder, void* renderPassDescriptor);

    void drawUI(Scene& scene, uint64_t frame, float fps, float frameTimeMs, size_t renderCommandCount);

    void setRenderer(MetalRenderer* renderer) { renderer_ = renderer; }
    void setLightSettings(LightSettings* settings) { lightSettings_ = settings; }
    void setSkySettings(SkySettings* settings) { skySettings_ = settings; }

    const GameObject* selected() const { return selected_; }
    void setSelected(GameObject* obj) { selected_ = obj; }

private:
    void drawMainMenuBar(Scene& scene);
    void drawHierarchy(Scene& scene, float fps, float frameTimeMs);
    void drawHierarchyNode(Scene& scene, GameObject* obj);
    void drawInspector();
    void drawDiagnostics(uint64_t frame, float fps, float frameTimeMs, size_t renderCommandCount, Scene& scene);
    void drawAgentConsole(Scene& scene, uint64_t frame, float fps, float frameTimeMs, size_t renderCommandCount);
    void drawScriptRunner(Scene& scene);
    void drawScreenshotPanel(Scene& scene);
    void drawReproBundlePanel(Scene& scene, uint64_t frame, float fps, float frameTimeMs, size_t renderCommandCount);
    void drawLightingPanel();

    void loadWindowStates();
    void saveWindowStates();

public:
    // Persist current panel visibility. Safe to call while the editor is still
    // alive (e.g. from the platform close handler).
    void saveSettings() { saveWindowStates(); }

    // Persist current ImGui layout (panel positions / sizes).
    void saveLayout();

private:
    bool initialized_ = false;
    bool showHierarchy_ = true;
    bool showInspector_ = true;
    bool showLighting_ = true;
    bool showDiagnostics_ = true;
    bool showAgentConsole_ = true;
    bool showScriptRunner_ = true;
    bool showScreenshot_ = true;
    bool showReproBundle_ = true;
    GameObject* selected_ = nullptr;
    MetalRenderer* renderer_ = nullptr;
    LightSettings* lightSettings_ = nullptr;
    SkySettings* skySettings_ = nullptr;
    char nameBuffer_[128] = {};
    char commandBuffer_[256] = {};
    std::string consoleOutput_;
    char scriptBuffer_[128] = {};
    std::string scriptOutput_;
    std::string lastScriptPath_;
    char screenshotBuffer_[128] = {};
    std::string screenshotOutput_;
    std::string lastCapturePath_;
    char bundleBuffer_[128] = {};
    std::string bundleOutput_;
    std::string lastBundlePath_;
    std::string lastAssertionFailure_;
    std::string imguiIniPath_;
};
