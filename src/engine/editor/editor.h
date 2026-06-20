#pragma once

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

    void drawUI(Scene& scene, float fps, float frameTimeMs);

private:
    void drawHierarchy(Scene& scene, float fps, float frameTimeMs);
    void drawInspector();

    bool initialized_ = false;
    GameObject* selected_ = nullptr;
    char nameBuffer_[128] = {};
};
