#include "renderer/render_context.h"

void RenderContext::setCamera(const Mat4& view, const Mat4& projection)
{
    view_ = view;
    projection_ = projection;
}

void RenderContext::setLight(const Vec3& direction, float ambient, float diffuse)
{
    Vec3 n = normalize(direction);
    light_.direction = {n.x, n.y, n.z};
    light_.ambient = ambient;
    light_.diffuse = diffuse;
}

void RenderContext::setSky(const SkySettings& sky)
{
    sky_ = sky;
}

void RenderContext::setShadow(const Mat4& lightViewProjection, bool enabled)
{
    lightViewProjection_ = lightViewProjection;
    shadowsEnabled_ = enabled;
}

void RenderContext::drawShape(ShapeType shape, const Mat4& transform, const simd::float4& color, bool highlight)
{
    RenderCommand cmd;
    cmd.type = shape;
    cmd.transform = transform;
    cmd.color = color;
    cmd.highlight = highlight;
    commands_.push_back(cmd);
}

void RenderContext::drawGround(const Vec3& cameraPos, float gridScale, float gridMajorDiv)
{
    RenderCommand cmd;
    cmd.type = ShapeType::Ground;
    cmd.transform = identity();
    cmd.gridScale = gridScale;
    cmd.gridMajorDiv = gridMajorDiv;
    cmd.cameraPos[0] = cameraPos.x;
    cmd.cameraPos[1] = cameraPos.y;
    cmd.cameraPos[2] = cameraPos.z;
    commands_.push_back(cmd);
}
