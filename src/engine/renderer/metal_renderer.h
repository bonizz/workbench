#pragma once

#include "render_context.h"

#include <functional>

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

private:
    struct Impl;
    Impl* impl_;
};
