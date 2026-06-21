#include "core/picking.h"

#include <cmath>
#include <limits>

namespace {

constexpr float kEpsilon = 1e-6f;

} // namespace

Ray makeCameraRay(const Vec3& cameraPosition,
                  const Mat4& viewMatrix,
                  float aspect,
                  float fovY,
                  float ndcX,
                  float ndcY)
{
    // lookAt stores the orthonormal basis as the rows of the upper 3x3.
    // Use the canonical helpers so the extraction is not open-coded.
    Vec3 right = cameraRightFromView(viewMatrix);
    Vec3 up = cameraUpFromView(viewMatrix);
    Vec3 backward = cameraBackwardFromView(viewMatrix);

    float tanHalfFov = std::tan(fovY * 0.5f);
    float halfH = tanHalfFov;        // near-plane distance cancels in normalization
    float halfW = aspect * halfH;

    Vec3 dir = {
        right.x * (ndcX * halfW) + up.x * (ndcY * halfH) - backward.x,
        right.y * (ndcX * halfW) + up.y * (ndcY * halfH) - backward.y,
        right.z * (ndcX * halfW) + up.z * (ndcY * halfH) - backward.z
    };

    return {cameraPosition, normalize(dir)};
}

bool intersectRaySphere(const Vec3& origin, const Vec3& dir, float radius, float& t)
{
    float a = dot(dir, dir);
    float b = 2.0f * dot(origin, dir);
    float c = dot(origin, origin) - radius * radius;
    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) {
        return false;
    }

    float sqrtDisc = std::sqrt(disc);
    float t1 = (-b - sqrtDisc) / (2.0f * a);
    float t2 = (-b + sqrtDisc) / (2.0f * a);

    if (t1 > 0.0f) {
        t = t1;
        return true;
    }
    if (t2 > 0.0f) {
        t = t2;
        return true;
    }
    return false;
}

bool intersectRayAABB(const Vec3& origin,
                      const Vec3& dir,
                      const Vec3& min,
                      const Vec3& max,
                      float& t)
{
    float tmin = -std::numeric_limits<float>::infinity();
    float tmax = std::numeric_limits<float>::infinity();

    for (int axis = 0; axis < 3; ++axis) {
        float o = (axis == 0) ? origin.x : (axis == 1) ? origin.y : origin.z;
        float d = (axis == 0) ? dir.x : (axis == 1) ? dir.y : dir.z;
        float mn = (axis == 0) ? min.x : (axis == 1) ? min.y : min.z;
        float mx = (axis == 0) ? max.x : (axis == 1) ? max.y : max.z;

        if (std::fabs(d) < kEpsilon) {
            if (o < mn || o > mx) {
                return false;
            }
        } else {
            float t1 = (mn - o) / d;
            float t2 = (mx - o) / d;
            if (t1 > t2) {
                std::swap(t1, t2);
            }
            if (t1 > tmin) {
                tmin = t1;
            }
            if (t2 < tmax) {
                tmax = t2;
            }
            if (tmin > tmax) {
                return false;
            }
        }
    }

    if (tmin > 0.0f) {
        t = tmin;
        return true;
    }
    if (tmax > 0.0f) {
        t = tmax;
        return true;
    }
    return false;
}

bool intersectRayPlane(const Vec3& origin, const Vec3& dir, float& t)
{
    if (std::fabs(dir.y) < kEpsilon) {
        return false;
    }

    t = -origin.y / dir.y;
    if (t < 0.0f) {
        return false;
    }

    float hx = origin.x + t * dir.x;
    float hz = origin.z + t * dir.z;
    if (hx < -1.0f || hx > 1.0f || hz < -1.0f || hz > 1.0f) {
        return false;
    }

    return true;
}
