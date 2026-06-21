#pragma once

#include "core/math.h"

struct Ray
{
    Vec3 origin;
    Vec3 direction;
};

// Builds a world-space ray from a viewport click. The ray origin is the camera
// position; the direction is derived from the camera's view matrix (which stores
// the orthonormal right/up/backward basis as the rows of its upper 3x3) and the
// NDC position of the click. This avoids duplicating camera orientation math or
// inverting the projection matrix.
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

// Translate gizmo (milestone 0.11). The handle is a small sphere drawn at the
// selected object's origin; the same radius is used to draw it and to pick it.
constexpr float kGizmoHandleRadius = 0.25f;

// Intersect a ray with the unbounded horizontal plane y = planeY. Returns false
// if the ray is parallel to the plane or the hit is behind the origin. On hit,
// fills hitPoint. Unlike intersectRayPlane this has no x/z bounds and supports
// an arbitrary height, which the gizmo drag needs.
bool intersectRayHorizontalPlane(const Vec3& origin,
                                 const Vec3& dir,
                                 float planeY,
                                 Vec3& hitPoint);

// Gizmo drag move: keep Y, apply the XZ offset captured at drag start. The
// caller captures dragOffset = objectPos - hitPoint (XZ only) when the drag
// begins; this reconstructs the new position from a later plane hit.
Vec3 planeDragPosition(const Vec3& hitPoint, const Vec3& dragOffset, float preservedY);
