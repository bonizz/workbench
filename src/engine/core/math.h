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
