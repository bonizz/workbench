#pragma once

#include "scene/transform.h"
#include <simd/simd.h>
#include <string>

class GameObject
{
public:
    explicit GameObject(std::string name);

    const std::string& name() const { return name_; }
    Transform& transform() { return transform_; }
    const Transform& transform() const { return transform_; }

    bool active() const { return active_; }
    void setActive(bool active) { active_ = active; }

    // Placeholder for future material/component system.
    simd::float4 color = {1.0f, 1.0f, 1.0f, 1.0f};

private:
    std::string name_;
    Transform transform_;
    bool active_ = true;
};
