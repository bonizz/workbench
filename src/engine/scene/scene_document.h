#pragma once

#include "core/math.h"
#include "renderer/render_types.h"

constexpr int kSceneVersion = 1;
constexpr float kDefaultMoveSpeed = 5.0f;

// Snapshot of the non-object portion of a scene file. Plain data, no behavior.
// The serializer walks Scene directly for the object array.
struct SceneCameraState
{
    Vec3 position{0.0f, 3.0f, 5.0f};
    Vec3 rotation{0.0f, 0.0f, 0.0f}; // radians
    float moveSpeed = kDefaultMoveSpeed;
};

struct SceneEnvironment
{
    LightSettings light;
    SkySettings sky;
};

struct SceneSettings
{
    // Intentionally empty for v1; scene-level knobs (grid, gravity, etc.)
    // belong here instead of leaking into app settings.
};

struct SceneDocument
{
    int version = kSceneVersion;
    SceneCameraState camera;
    SceneEnvironment environment;
    SceneSettings settings;
};
