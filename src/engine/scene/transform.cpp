#include "scene/transform.h"

Mat4 Transform::localMatrix() const
{
    Mat4 t = translation(position.x, position.y, position.z);
    Mat4 rx = rotationX(rotation.x);
    Mat4 ry = rotationY(rotation.y);
    Mat4 rz = rotationZ(rotation.z);
    Mat4 sm = ::scale(scale.x, scale.y, scale.z);
    return multiply(t, multiply(ry, multiply(rx, multiply(rz, sm))));
}
