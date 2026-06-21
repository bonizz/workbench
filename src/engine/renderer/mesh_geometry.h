#pragma once

#include "renderer/render_types.h"

#include <cstdint>
#include <vector>

struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

// Procedurally generated primitives centered at the origin with half-extent 1.
// All vertices are white (Vertex.color is unused by the shader; kept only to
// preserve the existing Vertex layout).
MeshData makeCube();
MeshData makeSphere();
MeshData makePlane();
