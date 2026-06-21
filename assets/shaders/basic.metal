#include <metal_stdlib>
using namespace metal;

struct Vertex
{
    packed_float3 position;
    packed_float4 color;
};

struct Uniforms
{
    float4x4 modelMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
};

struct VertexOut
{
    float4 position [[position]];
    float4 color;
};

vertex VertexOut vertex_main(const device Vertex* vertices [[buffer(0)]],
                             constant Uniforms& uniforms [[buffer(1)]],
                             uint vertexID [[vertex_id]])
{
    Vertex in = vertices[vertexID];

    float4 worldPosition = uniforms.modelMatrix * float4(in.position, 1.0f);
    float4 viewPosition = uniforms.viewMatrix * worldPosition;

    VertexOut out;
    out.position = uniforms.projectionMatrix * viewPosition;
    out.color = in.color;

    return out;
}

fragment float4 fragment_main(VertexOut in [[stage_in]],
                              constant float4& uColor [[buffer(0)]])
{
    return uColor;
}
