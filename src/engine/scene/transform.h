#pragma once

#include "core/math.h"

class Transform
{
public:
    // All three fields are LOCAL to this object's parent (or to the world
    // if it is a root). They are the source of truth; the world matrix below
    // is a derived cache.
    Vec3 position = {0.0f, 0.0f, 0.0f};
    Vec3 rotation = {0.0f, 0.0f, 0.0f}; // Euler angles in radians
    Vec3 scale    = {1.0f, 1.0f, 1.0f};

    // T * Ry * Rx * Rz * S, in this object's local space.
    Mat4 localMatrix() const;

    // Derived world matrix = parent.worldMatrix * localMatrix.
    // Refreshed by Scene::updateWorldTransforms(). Readers always refresh
    // before reading (see docs/conventions.md and the 0.6 plan).
    const Mat4& worldMatrix() const { return worldMatrix_; }

private:
    friend class Scene;
    Mat4 worldMatrix_ = identity();
};
