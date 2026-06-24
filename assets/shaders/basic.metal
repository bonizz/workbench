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
    float3 worldPos;
};

struct LightSettings
{
    float3 direction;
    float ambient;
    float diffuse;
};

struct ShadowData
{
    float4x4 lightViewProjection;
    float bias;
    float enabled;
    float2 pad;
};

// Returns 1.0 when the world point is lit, 0.0 when fully shadowed. Reversed-Z:
// the shadow map stores the largest depth (closest to the light), so a fragment
// is lit when its own light-space depth is >= the stored depth (minus an
// epsilon bias to suppress self-shadow acne).
float shadowFactor(float3 worldPos, ShadowData shadow, depth2d<float> shadowMap)
{
    if (shadow.enabled < 0.5f) {
        return 1.0f;
    }

    float4 lightClip = shadow.lightViewProjection * float4(worldPos, 1.0f);
    if (lightClip.w <= 0.0f) {
        return 1.0f;
    }
    float3 ndc = lightClip.xyz / lightClip.w;

    // NDC [-1,1] -> texture UV [0,1], flipping Y (Metal texture origin is top-left).
    float2 uv = ndc.xy * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) {
        return 1.0f;
    }

    constexpr sampler shadowSampler(coord::normalized, filter::nearest, address::clamp_to_edge);
    float storedDepth = shadowMap.sample(shadowSampler, uv);

    return (ndc.z >= storedDepth - shadow.bias) ? 1.0f : 0.0f;
}

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
    out.worldPos = worldPosition.xyz;

    return out;
}

fragment float4 fragment_main(VertexOut in [[stage_in]],
                              constant float4& uColor [[buffer(0)]],
                              constant LightSettings& light [[buffer(1)]],
                              constant ShadowData& shadow [[buffer(2)]],
                              depth2d<float> shadowMap [[texture(0)]])
{
    float3 n = normalize(in.worldNormal);
    // light.direction is the direction light travels; the surface-to-light
    // vector is its negation. This matches the sky's sun direction
    // (sunDir = -light.direction), so the lit face faces the visible sun.
    float3 l = normalize(-light.direction);
    float lambert = max(dot(n, l), 0.0f);
    float shadowed = shadowFactor(in.worldPos, shadow, shadowMap);
    float lit = light.ambient + light.diffuse * lambert * shadowed;
    return float4(uColor.rgb * lit, uColor.a);
}
