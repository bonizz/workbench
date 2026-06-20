#pragma once

#include "scene/component.h"

#include <simd/simd.h>
#include <string>

namespace scene {

class MeshRenderer : public Component
{
public:
    const char* typeName() const override { return "MeshRenderer"; }
    std::unique_ptr<Component> clone() const override;

    std::string mesh = "cube";
    simd::float4 color = {1.0f, 1.0f, 1.0f, 1.0f};
};

} // namespace scene
