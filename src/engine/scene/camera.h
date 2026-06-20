#pragma once

#include "core/math.h"

class Camera
{
public:
    Vec3 position = {0.0f, 3.0f, 5.0f};
    float yaw = 0.0f;
    float pitch = -0.35f;

    Vec3 forward() const;
    Vec3 right() const;

    Mat4 viewMatrix() const;
};
