#pragma once

class Scene;

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

    void drawPanels(Scene& scene, float fps, float frameTimeMs);

private:
    bool initialized_ = false;
};
