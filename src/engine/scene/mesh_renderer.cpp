#include "scene/mesh_renderer.h"

#include <string>

namespace scene {

const char* meshShapeToString(MeshShape shape)
{
    switch (shape) {
        case MeshShape::Cube:   return "cube";
        case MeshShape::Sphere: return "sphere";
        case MeshShape::Plane:  return "plane";
    }
    return "cube";
}

bool meshShapeFromString(const std::string& name, MeshShape& out)
{
    if (name == "cube") {
        out = MeshShape::Cube;
        return true;
    }
    if (name == "sphere") {
        out = MeshShape::Sphere;
        return true;
    }
    if (name == "plane") {
        out = MeshShape::Plane;
        return true;
    }
    return false;
}

std::unique_ptr<Component> MeshRenderer::clone() const
{
    return std::make_unique<MeshRenderer>(*this);
}

} // namespace scene
