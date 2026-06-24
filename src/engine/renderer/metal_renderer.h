#pragma once

#include "render_context.h"

#include <functional>
#include <string>

class MetalRenderer
{
public:
    using UIRenderCallback = std::function<void(void* commandBuffer, void* renderEncoder, void* renderPassDescriptor)>;

    explicit MetalRenderer(void* nativeMetalLayer);
    ~MetalRenderer();

    MetalRenderer(const MetalRenderer&) = delete;
    MetalRenderer& operator=(const MetalRenderer&) = delete;

    void resize(float width, float height, float scale);
    void draw(const RenderContext& context, const float clearColor[4], const UIRenderCallback& uiCallback = nullptr);

    float aspectRatio() const;
    void* device() const;

    // The shadow-map debug color texture (depth copied to grayscale), suitable
    // for display via ImGui::Image. Returns an id<MTLTexture> or nullptr.
    void* shadowMapTexture() const;

    // Queues a screenshot of the next completed frame. The PNG is written
    // asynchronously from the command buffer completion handler.
    void requestScreenshot(const std::string& path);

private:
    struct Impl;
    Impl* impl_;
};
