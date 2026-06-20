#pragma once

#include "core/math.h"

class Transform
{
public:
    Vec3 position = {0.0f, 0.0f, 0.0f};
    Vec3 rotation = {0.0f, 0.0f, 0.0f}; // Euler angles in radians
    Vec3 scale    = {1.0f, 1.0f, 1.0f};

    Mat4 localMatrix() const;
};
