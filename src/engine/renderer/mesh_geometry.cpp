#include "renderer/mesh_geometry.h"

#include "core/math.h"

#include <cmath>

namespace {

constexpr int kSphereSlices = 16;
constexpr int kSphereStacks = 12;

Vertex whiteVertex(const Vec3& pos)
{
    return Vertex{{pos.x, pos.y, pos.z}, {1.0f, 1.0f, 1.0f, 1.0f}};
}

} // namespace

MeshData makeCube()
{
    MeshData data;
    data.vertices.reserve(8);
    data.indices.reserve(36);

    static const Vec3 positions[8] = {
        {-1.0f, -1.0f, -1.0f},
        { 1.0f, -1.0f, -1.0f},
        { 1.0f,  1.0f, -1.0f},
        {-1.0f,  1.0f, -1.0f},
        {-1.0f, -1.0f,  1.0f},
        { 1.0f, -1.0f,  1.0f},
        { 1.0f,  1.0f,  1.0f},
        {-1.0f,  1.0f,  1.0f},
    };

    for (const auto& p : positions) {
        data.vertices.push_back(whiteVertex(p));
    }

    static const uint16_t indices[36] = {
        0, 1, 2,   0, 2, 3,
        4, 6, 5,   4, 7, 6,
        3, 2, 6,   3, 6, 7,
        0, 4, 5,   0, 5, 1,
        1, 5, 6,   1, 6, 2,
        0, 3, 7,   0, 7, 4
    };

    for (uint16_t i : indices) {
        data.indices.push_back(i);
    }

    return data;
}

MeshData makeSphere()
{
    MeshData data;
    data.vertices.reserve((kSphereStacks + 1) * (kSphereSlices + 1));
    data.indices.reserve(kSphereStacks * kSphereSlices * 6);

    for (int stack = 0; stack <= kSphereStacks; ++stack) {
        float theta = kPi * static_cast<float>(stack) / static_cast<float>(kSphereStacks);
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        for (int slice = 0; slice <= kSphereSlices; ++slice) {
            float phi = 2.0f * kPi * static_cast<float>(slice) / static_cast<float>(kSphereSlices);
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            Vec3 pos = {
                sinTheta * cosPhi,
                cosTheta,
                sinTheta * sinPhi
            };
            data.vertices.push_back(whiteVertex(pos));
        }
    }

    for (int stack = 0; stack < kSphereStacks; ++stack) {
        for (int slice = 0; slice < kSphereSlices; ++slice) {
            uint16_t a = static_cast<uint16_t>(stack * (kSphereSlices + 1) + slice);
            uint16_t b = static_cast<uint16_t>((stack + 1) * (kSphereSlices + 1) + slice);
            uint16_t c = static_cast<uint16_t>((stack + 1) * (kSphereSlices + 1) + (slice + 1));
            uint16_t d = static_cast<uint16_t>(stack * (kSphereSlices + 1) + (slice + 1));

            data.indices.push_back(a);
            data.indices.push_back(b);
            data.indices.push_back(d);

            data.indices.push_back(b);
            data.indices.push_back(c);
            data.indices.push_back(d);
        }
    }

    return data;
}

MeshData makePlane()
{
    MeshData data;
    data.vertices.reserve(4);
    data.indices.reserve(6);

    data.vertices.push_back(whiteVertex({-1.0f, 0.0f, -1.0f}));
    data.vertices.push_back(whiteVertex({ 1.0f, 0.0f, -1.0f}));
    data.vertices.push_back(whiteVertex({ 1.0f, 0.0f,  1.0f}));
    data.vertices.push_back(whiteVertex({-1.0f, 0.0f,  1.0f}));

    // Winding is CCW when viewed from +Y. Culling is currently disabled, so the
    // single-sided quad is visible from below as well.
    data.indices = {0, 2, 1, 0, 3, 2};

    return data;
}
