#pragma once

#include "render_command.h"
#include "render_types.h"
#include "core/math.h"

#include <vector>

class RenderContext
{
public:
    void setCamera(const Mat4& view, const Mat4& projection);
    void setLight(const Vec3& direction, float ambient, float diffuse);
    void setSky(const SkySettings& sky);

    void drawShape(ShapeType shape, const Mat4& transform, const simd::float4& color, bool highlight = false);
    void drawGround(const Vec3& cameraPos, float gridScale = 1.0f, float gridMajorDiv = 10.0f);

    const Mat4& view() const { return view_; }
    const Mat4& projection() const { return projection_; }
    const LightSettings& light() const { return light_; }
    const SkySettings& sky() const { return sky_; }
    const std::vector<RenderCommand>& commands() const { return commands_; }

private:
    Mat4 view_ = simd::float4x4(1.0f);
    Mat4 projection_ = simd::float4x4(1.0f);
    LightSettings light_;
    SkySettings sky_;
    std::vector<RenderCommand> commands_;
};
