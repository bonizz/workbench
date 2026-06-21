#pragma once

#include "agent/test_suite.h"
#include "core/cli_options.h"
#include "core/input_state.h"
#include "core/time.h"
#include "renderer/render_types.h"
#include "scene/camera.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class Window;
class MetalRenderer;
class Editor;
class Scene;

enum class AutomationState
{
    Pending,
    RanCommands,
    WaitingExtraFrames,
    Done
};

class Application
{
public:
    Application();
    ~Application();

    bool init();
    int run();
    void shutdown();

    void setCliOptions(const CliOptions& options);
    int exitCode() const;
    void saveWindowSize();

    // Called by the platform window each frame.
    void onUpdate(float deltaTime);
    void onRender();
    void onResize(float width, float height, float scale);

    uint64_t frame() const { return frame_; }
    size_t lastRenderCommandCount() const { return lastRenderCommandCount_; }

    bool liveSimulation() const { return liveSimulation_; }

    LightSettings& lightSettings() { return lightSettings_; }
    const LightSettings& lightSettings() const { return lightSettings_; }

    // Input events from the platform window.
    void onKeyEvent(int keyCode, bool down);
    void onMouseDrag(float deltaX, float deltaY);
    void onScroll(float delta);

private:
    void recreateScene();
    void runAutomation();
    void runTestSuite();
    void waitForPendingScreenshot();

    std::unique_ptr<Window> window_;
    std::unique_ptr<MetalRenderer> renderer_;
    std::unique_ptr<Editor> editor_;
    std::unique_ptr<Scene> scene_;

    Time time_;

    bool keyW_ = false;
    bool keyA_ = false;
    bool keyS_ = false;
    bool keyD_ = false;
    bool keyQ_ = false;
    bool keyE_ = false;

    float mouseDeltaX_ = 0.0f;
    float mouseDeltaY_ = 0.0f;
    float scrollDelta_ = 0.0f;

    float clearColor_[4] = {0.03f, 0.05f, 0.08f, 1.0f};

    float fps_ = 0.0f;
    float frameTimeMs_ = 0.0f;

    uint64_t frame_ = 0;
    size_t lastRenderCommandCount_ = 0;

    CliOptions cliOptions_;
    AutomationState automationState_ = AutomationState::Pending;
    int automationWaitFrames_ = 0;
    std::string pendingScreenshotPath_;

    // True only in interactive (no automation) mode. When false, the
    // simulation advances exclusively via `sim.step` so scripts and tests
    // are deterministic and not polluted by per-frame real-time updates.
    bool liveSimulation_ = false;

    std::string lastAssertionFailure_;
    bool automationFailed_ = false;

    LightSettings lightSettings_;
};
