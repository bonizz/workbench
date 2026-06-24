#include <metal_stdlib>
using namespace metal;

// Depth -> color debug copy. ImGui's Metal backend samples user textures as
// texture2d<float>, so a raw Depth32Float shadow map cannot be displayed
// directly. This fullscreen pass reads the shadow depth and writes grayscale
// into a normal color texture that the editor can show with ImGui::Image.
//
// Reversed-Z: depth is ~1.0 near the light and ~0.0 far away, so occluders
// closest to the light appear brightest.

struct DebugOut
{
    float4 position [[position]];
    float2 uv;
};

vertex DebugOut shadow_debug_vertex(uint vertexID [[vertex_id]])
{
    // Fullscreen triangle.
    float2 positions[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
    float2 uvs[3]       = { float2(0.0, 1.0),  float2(2.0, 1.0),  float2(0.0, -1.0) };

    DebugOut out;
    out.position = float4(positions[vertexID], 0.0, 1.0);
    out.uv = uvs[vertexID];
    return out;
}

fragment float4 shadow_debug_fragment(DebugOut in [[stage_in]],
                                      depth2d<float> shadowMap [[texture(0)]])
{
    constexpr sampler s(coord::normalized, filter::nearest, address::clamp_to_edge);
    float d = shadowMap.sample(s, in.uv);
    return float4(d, d, d, 1.0);
}
