#pragma once

#include "scene/component.h"
#include "core/math.h"

#include <memory>

namespace scene {

// A simple behavior component that rotates its owning GameObject.
// angularVelocityEuler is in degrees per second, per Euler axis.
// onUpdate converts deg/s to rad/s and adds to Transform::rotation (radians).
class RotateComponent : public Component
{
public:
    const char* typeName() const override { return "RotateComponent"; }
    std::unique_ptr<Component> clone() const override;
    void onUpdate(float deltaTime) override;

    Vec3 angularVelocityEuler{0.0f, 0.0f, 0.0f};
};

} // namespace scene
