#pragma once

#include <simd/simd.h>
#include <cstdint>

// CPU-side vertex layout. Must match the Metal shader:
//   struct Vertex { packed_float3 position; packed_float3 normal; };
struct Vertex
{
    float position[3];
    float normal[3];
};

static_assert(sizeof(Vertex) == 24, "Vertex size mismatch");
static_assert(alignof(Vertex) == 4, "Vertex alignment mismatch");

// Must match the Metal shader Uniforms struct.
struct alignas(16) ShaderUniforms
{
    simd::float4x4 modelMatrix;
    simd::float4x4 viewMatrix;
    simd::float4x4 projectionMatrix;
    simd::float3x3 normalMatrix;
};

static_assert(sizeof(ShaderUniforms) == 240, "ShaderUniforms size mismatch");

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
    Light = 1,
};

enum class SkyFragmentBufferIndex : uint32_t
{
    SkySettings = 0,
    InvViewProj = 1,
    CameraPos   = 2,
    SunDir      = 3,
};

// Global directional light settings. Must match the Metal shader LightSettings struct.
struct alignas(16) LightSettings
{
    simd::float3 direction = {0.0f, -1.0f, 0.0f};
    float ambient = 0.3f;
    float diffuse = 0.7f;
};

static_assert(sizeof(LightSettings) == 32, "LightSettings size mismatch");

// Procedural sky settings. Must match the Metal shader SkySettings struct.
struct alignas(16) SkySettings
{
    simd::float3 horizonColor = {0.55f, 0.65f, 0.78f};
    simd::float3 zenithColor  = {0.12f, 0.22f, 0.45f};
    simd::float3 sunColor     = {1.0f, 0.95f, 0.85f};
    float sunSize      = 0.04f;
    float sunIntensity = 1.0f;
};

static_assert(sizeof(SkySettings) == 64, "SkySettings size mismatch");
