#pragma once

#include "render_command.h"
#include "core/math.h"

#include <vector>

class RenderContext
{
public:
    void setCamera(const Mat4& view, const Mat4& projection);

    void drawShape(ShapeType shape, const Mat4& transform, const simd::float4& color);
    void drawGround(const Vec3& cameraPos, float gridScale = 1.0f, float gridMajorDiv = 10.0f);

    const Mat4& view() const { return view_; }
    const Mat4& projection() const { return projection_; }
    const std::vector<RenderCommand>& commands() const { return commands_; }

private:
    Mat4 view_ = simd::float4x4(1.0f);
    Mat4 projection_ = simd::float4x4(1.0f);
    std::vector<RenderCommand> commands_;
};
