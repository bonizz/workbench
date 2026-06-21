#pragma once

#include <simd/simd.h>
#include <cstdint>

// CPU-side vertex layout. Must match the Metal shader:
//   struct Vertex { packed_float3 position; packed_float4 color; };
struct Vertex
{
    float position[3];
    float color[4];
};

static_assert(sizeof(Vertex) == 28, "Vertex size mismatch");
static_assert(alignof(Vertex) == 4, "Vertex alignment mismatch");

// Must match the Metal shader Uniforms struct (float4x4 x 3).
struct alignas(16) ShaderUniforms
{
    simd::float4x4 modelMatrix;
    simd::float4x4 viewMatrix;
    simd::float4x4 projectionMatrix;
};

static_assert(sizeof(ShaderUniforms) == 192, "ShaderUniforms size mismatch");

enum class VertexBufferIndex : uint32_t
{
    Vertices = 0,
    Uniforms = 1,
};

enum class FragmentBufferIndex : uint32_t
{
    GridScale = 0,
    GridMajorDiv = 1,
    CameraPos = 2,
};

enum class MeshFragmentBufferIndex : uint32_t
{
    Color = 0,
};
