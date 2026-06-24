#include <metal_stdlib>
using namespace metal;

// Depth-only shadow pass. Renders occluders from the directional light's point
// of view into a Depth32Float texture. There is no fragment function: the
// pipeline writes depth only. Reversed-Z is in effect (near -> 1, far -> 0),
// matching the main scene pass, so the existing depth state is reused.

struct Vertex
{
    packed_float3 position;
    packed_float3 normal;
};

struct ShadowUniforms
{
    float4x4 modelMatrix;
    float4x4 lightViewProjection;
};

vertex float4 shadow_vertex(const device Vertex* vertices [[buffer(0)]],
                            constant ShadowUniforms& uniforms [[buffer(1)]],
                            uint vertexID [[vertex_id]])
{
    Vertex in = vertices[vertexID];
    float4 worldPosition = uniforms.modelMatrix * float4(in.position, 1.0f);
    return uniforms.lightViewProjection * worldPosition;
}
