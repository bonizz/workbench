#pragma once

#include "core/math.h"
#include "core/object_id.h"
#include "scene/game_object.h"

struct InputState;

class Camera : public GameObject
{
public:
    Camera(ObjectId id, const std::string& name, const Vec3& position);

    void update(float deltaTime, const InputState& input);

    void setAspect(float aspect);

    Mat4 viewMatrix() const;

private:
    Vec3 forward() const;
    Vec3 right() const;

public:
    Mat4 projectionMatrix() const { return projection_; }

    float moveSpeed() const { return moveSpeed_; }

private:
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float moveSpeed_ = 5.0f;
    float rotationSpeed_ = 0.2f;
    Mat4 projection_ = identity();
};
