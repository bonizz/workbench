#include <metal_stdlib>
using namespace metal;

struct Vertex
{
    packed_float3 position;
    packed_float3 normal;
};

struct VertexOut
{
    float4 position [[position]];
    float3 worldPos;
};

struct Uniforms
{
    float4x4 modelMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
};

struct ShadowData
{
    float4x4 lightViewProjection;
    float bias;
    float enabled;
    float2 pad;
};

// See basic.metal for the reversed-Z shadow lookup rationale. Returns 1.0 lit,
// 0.0 shadowed.
float gridShadowFactor(float3 worldPos, ShadowData shadow, depth2d<float> shadowMap)
{
    if (shadow.enabled < 0.5f) {
        return 1.0f;
    }

    float4 lightClip = shadow.lightViewProjection * float4(worldPos, 1.0f);
    if (lightClip.w <= 0.0f) {
        return 1.0f;
    }
    float3 ndc = lightClip.xyz / lightClip.w;

    float2 uv = ndc.xy * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) {
        return 1.0f;
    }

    constexpr sampler shadowSampler(coord::normalized, filter::nearest, address::clamp_to_edge);
    float storedDepth = shadowMap.sample(shadowSampler, uv);

    return (ndc.z >= storedDepth - shadow.bias) ? 1.0f : 0.0f;
}

vertex VertexOut grid_vertex(const device Vertex* vertices [[buffer(0)]],
                             constant Uniforms& uniforms [[buffer(1)]],
                             uint vertexID [[vertex_id]])
{
    Vertex in = vertices[vertexID];
    VertexOut out;
    float4 worldPos = uniforms.modelMatrix * float4(in.position, 1.0);
    out.worldPos = worldPos.xyz;
    out.position = uniforms.projectionMatrix * uniforms.viewMatrix * worldPos;
    return out;
}

fragment float4 grid_fragment(VertexOut in [[stage_in]],
                              constant float& gridScale [[buffer(0)]],
                              constant float& majorDiv  [[buffer(1)]],
                              constant float3& cameraPos [[buffer(2)]],
                              constant ShadowData& shadow [[buffer(3)]],
                              depth2d<float> shadowMap [[texture(0)]])
{
    float2 coord = in.worldPos.xz / gridScale;

    // Anti-aliased minor grid lines
    float2 grid = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
    float minorLine = 1.0 - min(min(grid.x, grid.y), 1.0);

    // Anti-aliased major grid lines
    float2 majorCoord = in.worldPos.xz / (gridScale * majorDiv);
    float2 majorGrid = abs(fract(majorCoord - 0.5) - 0.5) / fwidth(majorCoord);
    float majorLine = 1.0 - min(min(majorGrid.x, majorGrid.y), 1.0);

    // Checkerboard cells
    float2 cell = floor(coord);
    float checker = fmod(cell.x + cell.y, 2.0);
    float3 darkCell  = float3(0.10, 0.10, 0.11);
    float3 lightCell = float3(0.14, 0.14, 0.15);
    float3 baseColor = mix(darkCell, lightCell, checker);

    float3 minorColor = float3(0.40, 0.40, 0.40);
    float3 majorColor = float3(0.70, 0.70, 0.70);

    float lineIntensity = max(minorLine, majorLine);
    float3 lineColor = mix(minorColor, majorColor, majorLine);
    float3 finalColor = mix(baseColor, lineColor, lineIntensity);

    // Origin axes (X=red, Z=blue) — consistent screen-space thickness
    float xAxis = 1.0 - min(abs(in.worldPos.x) / fwidth(in.worldPos.x), 1.0);
    float zAxis = 1.0 - min(abs(in.worldPos.z) / fwidth(in.worldPos.z), 1.0);
    float3 axisColor = mix(float3(0.9, 0.15, 0.15), float3(0.15, 0.35, 0.9), zAxis);
    finalColor = mix(finalColor, axisColor, max(xAxis, zAxis));

    // Origin highlight
    float originDist = length(in.worldPos.xz);
    float origin = 1.0 - min(originDist / fwidth(originDist), 1.0);
    finalColor = mix(finalColor, float3(1.0, 0.9, 0.3), origin * 0.8);

    // Directional shadow: darken the ground where it is occluded from the sun.
    float lit = gridShadowFactor(in.worldPos, shadow, shadowMap);
    finalColor *= mix(0.45, 1.0, lit);

    // Distance fade from camera
    float dist = length(in.worldPos - cameraPos);
    float fade = 1.0 - smoothstep(50.0, 90.0, dist);

    return float4(finalColor * fade, fade);
}
