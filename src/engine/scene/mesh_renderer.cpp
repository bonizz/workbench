#include "scene/mesh_renderer.h"

namespace scene {

std::unique_ptr<Component> MeshRenderer::clone() const
{
    return std::make_unique<MeshRenderer>(*this);
}

} // namespace scene
