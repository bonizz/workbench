#pragma once

#include "renderer/render_types.h"
#include "scene/scene_history.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Application;
class MetalRenderer;
class Scene;
class GameObject;

namespace scene { enum class MeshShape : int; }

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

    void setApplication(Application* app) { application_ = app; }
    void setRenderer(MetalRenderer* renderer) { renderer_ = renderer; }
    void setLightSettings(LightSettings* settings) { lightSettings_ = settings; }
    void setSkySettings(SkySettings* settings) { skySettings_ = settings; }

    const GameObject* selected() const { return selected_; }
    void setSelected(GameObject* obj) { selected_ = obj; }

    // v1 snapshot undo/redo (see docs/plans/0.15_undo_redo.md). Called by
    // Application on Cmd+Z / Cmd+Shift+Z. clearHistory is invoked on scene
    // load/new so undo can't cross a scene boundary.
    void undo(Scene& scene);
    void redo(Scene& scene);
    void clearHistory();
    // Brackets a viewport gizmo drag as a single coalesced undo entry.
    // beginDragEdit captures the pre-edit snapshot; endDragEdit commits it.
    // Called by Application at drag start (first move) and release.
    void beginDragEdit(Scene& scene);
    void endDragEdit();

private:
    void drawMainMenuBar(Scene& scene);
    void markSceneDirty();
    // v1 snapshot undo/redo helpers (see docs/plans/0.15_undo_redo.md).
    void pushUndo(Scene& scene);
    // Coalesces a continuous ImGui edit (drag/slider/color) into a single undo
    // entry: snapshot on item activation, commit on deactivate-after-edit.
    void coalesceEdit(Scene& scene);
    // Queues the Save As modal to open and prefills the filename field from the
    // scene's current name (or "untitled"). The popup itself is opened and drawn
    // from the top-level drawUI path, never from inside a menu block.
    void requestSaveAs(Scene& scene);
    void drawSaveAsModal(Scene& scene);
    // Queues the Open modal to open and refreshes the scene list from disk. As
    // with Save As, the popup is opened and drawn from the top-level drawUI
    // path, never from inside a menu block.
    void requestOpen();
    void drawOpenModal();
    void drawHierarchy(Scene& scene, float fps, float frameTimeMs);
    void drawHierarchyToolbar(Scene& scene);
    void createPrimitive(Scene& scene, scene::MeshShape shape, const char* name);
    void drawHierarchyNode(Scene& scene, GameObject* obj);
    void drawInspector(Scene& scene);
    void drawDiagnostics(uint64_t frame, float fps, float frameTimeMs, size_t renderCommandCount, Scene& scene);
    void drawAgentConsole(Scene& scene, uint64_t frame, float fps, float frameTimeMs, size_t renderCommandCount);
    void drawScriptRunner(Scene& scene);
    void drawScreenshotPanel(Scene& scene);
    void drawReproBundlePanel(Scene& scene, uint64_t frame, float fps, float frameTimeMs, size_t renderCommandCount);
    void drawLightingPanel(Scene& scene);

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
    Application* application_ = nullptr;
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
    bool saveAsPending_ = false;
    char saveAsBuffer_[256] = {};
    std::string saveAsError_;
    bool openPending_ = false;
    std::vector<std::string> openScenes_;
    int openSelected_ = -1;
    std::string openError_;
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
    SceneHistory history_;
};
