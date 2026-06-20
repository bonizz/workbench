#pragma once

#include "core/object_id.h"
#include "scene/transform.h"
#include <simd/simd.h>
#include <string>

class GameObject
{
public:
    GameObject(ObjectId id, std::string name);
    virtual ~GameObject() = default;

    ObjectId id() const { return id_; }

    const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    Transform& transform() { return transform_; }
    const Transform& transform() const { return transform_; }

    bool active() const { return active_; }
    void setActive(bool active) { active_ = active; }

    // Placeholder for future material/component system.
    simd::float4 color = {1.0f, 1.0f, 1.0f, 1.0f};

private:
    ObjectId id_;
    std::string name_;
    Transform transform_;
    bool active_ = true;
};
