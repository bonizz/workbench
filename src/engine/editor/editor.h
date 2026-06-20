#pragma once

#include <cstddef>
#include <cstdint>

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

    const GameObject* selected() const { return selected_; }

private:
    void drawHierarchy(Scene& scene, float fps, float frameTimeMs);
    void drawInspector();
    void drawDiagnostics(uint64_t frame, float fps, float frameTimeMs, size_t renderCommandCount, const Scene& scene);

    bool initialized_ = false;
    GameObject* selected_ = nullptr;
    char nameBuffer_[128] = {};
};
