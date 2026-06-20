#include "scene/camera.h"
#include "core/input_state.h"

#include <cmath>

namespace {

constexpr float kMaxPitch = 85.0f * 3.14159f / 180.0f;
constexpr float kMinMoveSpeed = 1.0f;
constexpr float kMaxMoveSpeed = 50.0f;

} // namespace

Camera::Camera(ObjectId id, const std::string& name, const Vec3& position)
    : GameObject(id, name)
{
    transform().position = position;
}

void Camera::update(float deltaTime, const InputState& input)
{
    // Sync orientation from the transform so inspector edits apply.
    yaw_ = transform().rotation.y;
    pitch_ = transform().rotation.x;

    // Mouse look.
    yaw_ += input.mouseDeltaX * rotationSpeed_ * 0.01f;
    pitch_ += input.mouseDeltaY * rotationSpeed_ * 0.01f;

    if (pitch_ > kMaxPitch) pitch_ = kMaxPitch;
    if (pitch_ < -kMaxPitch) pitch_ = -kMaxPitch;

    transform().rotation = {pitch_, yaw_, 0.0f};

    // Movement.
    Vec3 f = forward();
    Vec3 r = right();

    float forwardInput = (input.forward ? 1.0f : 0.0f) - (input.backward ? 1.0f : 0.0f);
    float rightInput   = (input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f);

    float speed = moveSpeed_ * deltaTime;
    transform().position.x += (f.x * forwardInput + r.x * rightInput) * speed;
    transform().position.y += (f.y * forwardInput + r.y * rightInput) * speed;
    transform().position.z += (f.z * forwardInput + r.z * rightInput) * speed;

    // Scroll adjusts movement speed.
    if (input.scrollDelta != 0.0f) {
        moveSpeed_ += input.scrollDelta;
        if (moveSpeed_ < kMinMoveSpeed) moveSpeed_ = kMinMoveSpeed;
        if (moveSpeed_ > kMaxMoveSpeed) moveSpeed_ = kMaxMoveSpeed;
    }
}

void Camera::setAspect(float aspect)
{
    projection_ = perspective(60.0f * 3.14159f / 180.0f, aspect, 0.1f, 100.0f);
}

Vec3 Camera::forward() const
{
    return normalize({
        std::cos(pitch_) * std::sin(yaw_),
        std::sin(pitch_),
        -std::cos(pitch_) * std::cos(yaw_)
    });
}

Vec3 Camera::right() const
{
    return normalize(cross(forward(), {0, 1, 0}));
}

Mat4 Camera::viewMatrix() const
{
    Vec3 eye = transform().position;
    Vec3 f = forward();
    Vec3 center = {eye.x + f.x, eye.y + f.y, eye.z + f.z};
    return lookAt(eye, center, {0, 1, 0});
}
