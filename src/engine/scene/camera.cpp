#include "scene/camera.h"

#include <cmath>

Vec3 Camera::forward() const
{
    return normalize({
        std::cos(pitch) * std::sin(yaw),
        std::sin(pitch),
        -std::cos(pitch) * std::cos(yaw)
    });
}

Vec3 Camera::right() const
{
    return normalize(cross(forward(), {0, 1, 0}));
}

Mat4 Camera::viewMatrix() const
{
    Vec3 eye = position;
    Vec3 f = forward();
    Vec3 center = {eye.x + f.x, eye.y + f.y, eye.z + f.z};
    return lookAt(eye, center, {0, 1, 0});
}
