#pragma once

#include <simd/simd.h>
#include <cmath>

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

using Mat4 = simd::float4x4;
using Mat3 = simd::float3x3;

constexpr float kPi = 3.14159265f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

Mat4 identity();
Mat4 translation(float x, float y, float z);
Mat4 rotationX(float radians);
Mat4 rotationY(float radians);
Mat4 rotationZ(float radians);
Mat4 scale(float x, float y, float z);
Mat4 multiply(const Mat4& left, const Mat4& right);

Mat4 perspective(float fovY, float aspect, float nearZ, float farZ);
Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up);

Vec3 normalize(Vec3 v);
Vec3 cross(Vec3 a, Vec3 b);
float dot(Vec3 a, Vec3 b);

// Inverse-transpose of the upper 3x3 of a 4x4 matrix, for transforming normals.
// Required for correct lighting under non-uniform scale.
Mat3 normalMatrix(const Mat4& m);

// Matrix inverse. Uses simd::inverse; valid for any invertible Mat4.
// Named inverseMatrix to avoid ADL conflict with simd::inverse on Mat4.
Mat4 inverseMatrix(const Mat4& m);

// Transform a point by a 4x4 matrix (w=1, perspective divide).
Vec3 transformPoint(const Mat4& m, const Vec3& p);

// Transform a direction vector by a 4x4 matrix (w=0).
Vec3 transformVector(const Mat4& m, const Vec3& v);

inline Vec3 toDegrees(const Vec3& radians)
{
    return {radians.x * kRadToDeg, radians.y * kRadToDeg, radians.z * kRadToDeg};
}
