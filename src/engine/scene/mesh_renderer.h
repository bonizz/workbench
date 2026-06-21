#pragma once

#include "scene/component.h"

#include <simd/simd.h>

namespace scene {

enum class MeshShape { Cube, Sphere, Plane };

// Lowercase canonical names: "cube", "sphere", "plane".
const char* meshShapeToString(MeshShape shape);
// Returns false on an unrecognized name (out is unchanged).
bool meshShapeFromString(const std::string& name, MeshShape& out);

class MeshRenderer : public Component
{
public:
    const char* typeName() const override { return "MeshRenderer"; }
    std::unique_ptr<Component> clone() const override;

    MeshShape shape = MeshShape::Cube;
    simd::float4 color = {1.0f, 1.0f, 1.0f, 1.0f};
};

} // namespace scene
