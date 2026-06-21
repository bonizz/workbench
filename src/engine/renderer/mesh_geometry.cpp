#include "renderer/mesh_geometry.h"

#include "core/math.h"

#include <cmath>

namespace {

constexpr int kSphereSlices = 16;
constexpr int kSphereStacks = 12;

Vertex litVertex(const Vec3& pos, const Vec3& normal)
{
    return Vertex{{pos.x, pos.y, pos.z}, {normal.x, normal.y, normal.z}};
}

} // namespace

MeshData makeCube()
{
    MeshData data;
    data.vertices.reserve(24);
    data.indices.reserve(36);

    // Each face has 4 unique vertices so face normals are constant across the face.
    // Winding is CCW when viewed from outside the cube.

    // +X face (normal +X)
    data.vertices.push_back(litVertex({ 1, -1, -1}, { 1,  0,  0}));
    data.vertices.push_back(litVertex({ 1,  1, -1}, { 1,  0,  0}));
    data.vertices.push_back(litVertex({ 1,  1,  1}, { 1,  0,  0}));
    data.vertices.push_back(litVertex({ 1, -1,  1}, { 1,  0,  0}));

    // -X face (normal -X)
    data.vertices.push_back(litVertex({-1, -1,  1}, {-1,  0,  0}));
    data.vertices.push_back(litVertex({-1,  1,  1}, {-1,  0,  0}));
    data.vertices.push_back(litVertex({-1,  1, -1}, {-1,  0,  0}));
    data.vertices.push_back(litVertex({-1, -1, -1}, {-1,  0,  0}));

    // +Y face (normal +Y)
    data.vertices.push_back(litVertex({-1,  1, -1}, { 0,  1,  0}));
    data.vertices.push_back(litVertex({-1,  1,  1}, { 0,  1,  0}));
    data.vertices.push_back(litVertex({ 1,  1,  1}, { 0,  1,  0}));
    data.vertices.push_back(litVertex({ 1,  1, -1}, { 0,  1,  0}));

    // -Y face (normal -Y)
    data.vertices.push_back(litVertex({-1, -1,  1}, { 0, -1,  0}));
    data.vertices.push_back(litVertex({-1, -1, -1}, { 0, -1,  0}));
    data.vertices.push_back(litVertex({ 1, -1, -1}, { 0, -1,  0}));
    data.vertices.push_back(litVertex({ 1, -1,  1}, { 0, -1,  0}));

    // +Z face (normal +Z)
    data.vertices.push_back(litVertex({-1, -1,  1}, { 0,  0,  1}));
    data.vertices.push_back(litVertex({ 1, -1,  1}, { 0,  0,  1}));
    data.vertices.push_back(litVertex({ 1,  1,  1}, { 0,  0,  1}));
    data.vertices.push_back(litVertex({-1,  1,  1}, { 0,  0,  1}));

    // -Z face (normal -Z)
    data.vertices.push_back(litVertex({ 1, -1, -1}, { 0,  0, -1}));
    data.vertices.push_back(litVertex({-1, -1, -1}, { 0,  0, -1}));
    data.vertices.push_back(litVertex({-1,  1, -1}, { 0,  0, -1}));
    data.vertices.push_back(litVertex({ 1,  1, -1}, { 0,  0, -1}));

    for (int face = 0; face < 6; ++face) {
        uint16_t base = static_cast<uint16_t>(face * 4);
        data.indices.push_back(base + 0);
        data.indices.push_back(base + 2);
        data.indices.push_back(base + 1);
        data.indices.push_back(base + 0);
        data.indices.push_back(base + 3);
        data.indices.push_back(base + 2);
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
            data.vertices.push_back(litVertex(pos, normalize(pos)));
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

    const Vec3 up = {0.0f, 1.0f, 0.0f};
    data.vertices.push_back(litVertex({-1.0f, 0.0f, -1.0f}, up));
    data.vertices.push_back(litVertex({ 1.0f, 0.0f, -1.0f}, up));
    data.vertices.push_back(litVertex({ 1.0f, 0.0f,  1.0f}, up));
    data.vertices.push_back(litVertex({-1.0f, 0.0f,  1.0f}, up));

    // Winding is CCW when viewed from +Y. Culling is currently disabled, so the
    // single-sided quad is visible from below as well.
    data.indices = {0, 2, 1, 0, 3, 2};

    return data;
}
