#pragma once

#include "core/math.h"

struct Ray
{
    Vec3 origin;
    Vec3 direction;
};

// Builds a world-space ray from a viewport click. The ray origin is the camera
// position; the direction is derived from the camera's view matrix (which
// provides the orthonormal right/up/backward basis) and the NDC position of
// the click. This avoids duplicating camera orientation math or inverting the
// projection matrix.
Ray makeCameraRay(const Vec3& cameraPosition,
                  const Mat4& viewMatrix,
                  float aspect,
                  float fovY,
                  float ndcX,
                  float ndcY);

// Intersect a ray with a sphere centered at the origin. Returns true and sets
// t to the nearest positive hit distance on hit.
bool intersectRaySphere(const Vec3& origin, const Vec3& dir, float radius, float& t);

// Intersect a ray with an axis-aligned box. min/max are the box corners.
bool intersectRayAABB(const Vec3& origin,
                      const Vec3& dir,
                      const Vec3& min,
                      const Vec3& max,
                      float& t);

// Intersect a ray with the unit plane primitive: y=0, x/z in [-1,1].
bool intersectRayPlane(const Vec3& origin, const Vec3& dir, float& t);
