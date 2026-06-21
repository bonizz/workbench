#include "core/math.h"

#include <cmath>

Vec3 normalize(Vec3 v)
{
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len == 0.0f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return {v.x / len, v.y / len, v.z / len};
}

Vec3 cross(Vec3 a, Vec3 b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Mat4 identity()
{
    return Mat4{
        simd::float4{1, 0, 0, 0},
        simd::float4{0, 1, 0, 0},
        simd::float4{0, 0, 1, 0},
        simd::float4{0, 0, 0, 1}
    };
}

Mat4 translation(float x, float y, float z)
{
    return Mat4{
        simd::float4{1, 0, 0, 0},
        simd::float4{0, 1, 0, 0},
        simd::float4{0, 0, 1, 0},
        simd::float4{x, y, z, 1}
    };
}

Mat4 rotationX(float radians)
{
    float c = std::cos(radians);
    float s = std::sin(radians);
    return Mat4{
        simd::float4{1, 0, 0, 0},
        simd::float4{0, c, s, 0},
        simd::float4{0, -s, c, 0},
        simd::float4{0, 0, 0, 1}
    };
}

Mat4 rotationY(float radians)
{
    float c = std::cos(radians);
    float s = std::sin(radians);
    return Mat4{
        simd::float4{c, 0, s, 0},
        simd::float4{0, 1, 0, 0},
        simd::float4{-s, 0, c, 0},
        simd::float4{0, 0, 0, 1}
    };
}

Mat4 rotationZ(float radians)
{
    float c = std::cos(radians);
    float s = std::sin(radians);
    return Mat4{
        simd::float4{ c, s, 0, 0},
        simd::float4{-s, c, 0, 0},
        simd::float4{ 0, 0, 1, 0},
        simd::float4{ 0, 0, 0, 1}
    };
}

Mat4 scale(float x, float y, float z)
{
    return Mat4{
        simd::float4{x, 0, 0, 0},
        simd::float4{0, y, 0, 0},
        simd::float4{0, 0, z, 0},
        simd::float4{0, 0, 0, 1}
    };
}

Mat4 multiply(const Mat4& left, const Mat4& right)
{
    return left * right;
}

Mat4 perspective(float fovY, float aspect, float nearZ, float farZ)
{
    float tanHalfFov = std::tan(fovY * 0.5f);
    // Reversed-Z: near -> 1.0, far -> 0.0 for better depth precision.
    return Mat4{
        simd::float4{1.0f / (aspect * tanHalfFov), 0, 0, 0},
        simd::float4{0, 1.0f / tanHalfFov, 0, 0},
        simd::float4{0, 0, nearZ / (farZ - nearZ), -1},
        simd::float4{0, 0, (nearZ * farZ) / (farZ - nearZ), 0}
    };
}

Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up)
{
    Vec3 f = normalize({eye.x - center.x, eye.y - center.y, eye.z - center.z});
    Vec3 s = normalize(cross(up, f));
    Vec3 u = cross(f, s);
    return Mat4{
        simd::float4{s.x, u.x, f.x, 0},
        simd::float4{s.y, u.y, f.y, 0},
        simd::float4{s.z, u.z, f.z, 0},
        simd::float4{-dot(s, eye), -dot(u, eye), -dot(f, eye), 1}
    };
}

Mat3 normalMatrix(const Mat4& m)
{
    Mat3 linear{m.columns[0].xyz, m.columns[1].xyz, m.columns[2].xyz};
    return simd::transpose(simd::inverse(linear));
}

Mat4 inverseMatrix(const Mat4& m)
{
    return simd::inverse(m);
}

Vec3 transformPoint(const Mat4& m, const Vec3& p)
{
    simd::float4 r = m * simd::float4{p.x, p.y, p.z, 1.0f};
    return {r.x / r.w, r.y / r.w, r.z / r.w};
}

Vec3 transformVector(const Mat4& m, const Vec3& v)
{
    simd::float4 r = m * simd::float4{v.x, v.y, v.z, 0.0f};
    return {r.x, r.y, r.z};
}
