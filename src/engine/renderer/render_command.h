#pragma once

#include <simd/simd.h>

enum class ShapeType
{
    Cube,   // Colored cube, dynamic vertex buffer
    Ground, // Infinite grid ground plane
};

struct RenderCommand
{
    ShapeType type = ShapeType::Cube;
    simd::float4x4 transform = simd::float4x4(1.0f);
    simd::float4 color = {1.0f, 1.0f, 1.0f, 1.0f};

    // Ground plane only
    float gridScale = 1.0f;
    float gridMajorDiv = 10.0f;
    float cameraPos[3] = {0.0f, 0.0f, 0.0f};
};
