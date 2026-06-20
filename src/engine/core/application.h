#pragma once

#include "core/input_state.h"
#include "core/time.h"
#include "scene/camera.h"

#include <cstddef>
#include <cstdint>
#include <memory>

class Window;
class MetalRenderer;
class Editor;
class Scene;

class Application
{
public:
    Application();
    ~Application();

    bool init();
    void run();
    void shutdown();

    // Called by the platform window each frame.
    void onUpdate(float deltaTime);
    void onRender();
    void onResize(float width, float height, float scale);

    uint64_t frame() const { return frame_; }
    size_t lastRenderCommandCount() const { return lastRenderCommandCount_; }

    // Input events from the platform window.
    void onKeyEvent(int keyCode, bool down);
    void onMouseDrag(float deltaX, float deltaY);
    void onScroll(float delta);

private:
    std::unique_ptr<Window> window_;
    std::unique_ptr<MetalRenderer> renderer_;
    std::unique_ptr<Editor> editor_;
    std::unique_ptr<Scene> scene_;

    Time time_;

    bool keyW_ = false;
    bool keyA_ = false;
    bool keyS_ = false;
    bool keyD_ = false;

    float mouseDeltaX_ = 0.0f;
    float mouseDeltaY_ = 0.0f;
    float scrollDelta_ = 0.0f;

    float clearColor_[4] = {0.03f, 0.05f, 0.08f, 1.0f};

    float fps_ = 0.0f;
    float frameTimeMs_ = 0.0f;

    uint64_t frame_ = 0;
    size_t lastRenderCommandCount_ = 0;
};
