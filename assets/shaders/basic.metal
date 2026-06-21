#include <metal_stdlib>
using namespace metal;

struct Vertex
{
    packed_float3 position;
    packed_float3 normal;
};

struct Uniforms
{
    float4x4 modelMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float3x3 normalMatrix;
};

struct VertexOut
{
    float4 position [[position]];
    float3 worldNormal;
};

struct LightSettings
{
    float3 direction;
    float ambient;
    float diffuse;
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
    out.worldNormal = uniforms.normalMatrix * float3(in.normal);

    return out;
}

fragment float4 fragment_main(VertexOut in [[stage_in]],
                              constant float4& uColor [[buffer(0)]],
                              constant LightSettings& light [[buffer(1)]])
{
    float3 n = normalize(in.worldNormal);
    // light.direction is the direction light travels; the surface-to-light
    // vector is its negation. This matches the sky's sun direction
    // (sunDir = -light.direction), so the lit face faces the visible sun.
    float3 l = normalize(-light.direction);
    float lambert = max(dot(n, l), 0.0f);
    float lit = light.ambient + light.diffuse * lambert;
    return float4(uColor.rgb * lit, uColor.a);
}
