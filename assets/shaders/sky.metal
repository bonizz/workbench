#include <metal_stdlib>
using namespace metal;

enum SkyFragmentBufferIndex : uint32_t
{
    SkyFragmentBufferIndexSkySettings = 0,
    SkyFragmentBufferIndexInvViewProj = 1,
    SkyFragmentBufferIndexCameraPos   = 2,
    SkyFragmentBufferIndexSunDir      = 3,
};

struct SkySettings
{
    float3 horizonColor;
    float3 zenithColor;
    float3 sunColor;
    float  sunSize;
    float  sunIntensity;
};

struct SkyOut
{
    float4 position [[position]];
    float2 ndc;
};

vertex SkyOut sky_vertex(uint vertexID [[vertex_id]])
{
    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0),
    };

    SkyOut out;
    out.position = float4(positions[vertexID], 0.0, 1.0);
    out.ndc = positions[vertexID];
    return out;
}

fragment float4 sky_fragment(SkyOut in [[stage_in]],
                             constant SkySettings& sky [[buffer(SkyFragmentBufferIndexSkySettings)]],
                             constant float4x4& invViewProj [[buffer(SkyFragmentBufferIndexInvViewProj)]],
                             constant float3& cameraPos [[buffer(SkyFragmentBufferIndexCameraPos)]],
                             constant float3& sunDir [[buffer(SkyFragmentBufferIndexSunDir)]])
{
    float4 worldPos = invViewProj * float4(in.ndc, 1.0, 1.0);
    worldPos /= worldPos.w;

    float3 rayDir = normalize(worldPos.xyz - cameraPos);

    float t = smoothstep(0.0, 1.0, clamp(rayDir.y, 0.0, 1.0));
    float3 color = mix(sky.horizonColor, sky.zenithColor, t);

    float d = dot(rayDir, sunDir);
    float ang = acos(clamp(d, -1.0, 1.0));
    float disk = 1.0 - smoothstep(sky.sunSize * 0.5, sky.sunSize, ang);
    float glow = (1.0 - smoothstep(sky.sunSize, sky.sunSize * 4.0, ang)) * 0.3;
    color += sky.sunColor * (disk + glow) * sky.sunIntensity;

    return float4(color, 1.0);
}
