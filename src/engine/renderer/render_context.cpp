#include "renderer/render_context.h"

void RenderContext::setCamera(const Mat4& view, const Mat4& projection)
{
    view_ = view;
    projection_ = projection;
}

void RenderContext::drawShape(ShapeType shape, const Mat4& transform, const simd::float4& color)
{
    RenderCommand cmd;
    cmd.type = shape;
    cmd.transform = transform;
    cmd.color = color;
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
