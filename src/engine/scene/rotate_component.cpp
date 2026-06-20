#include "scene/rotate_component.h"

#include "scene/game_object.h"
#include "scene/transform.h"

namespace scene {

std::unique_ptr<Component> RotateComponent::clone() const
{
    // Copy by value preserves angularVelocityEuler. The base Component copy
    // resets started_ to false (default), so the duplicate starts its own
    // lifecycle on the next Scene::update. This is intentional.
    return std::make_unique<RotateComponent>(*this);
}

void RotateComponent::onUpdate(float deltaTime)
{
    GameObject* go = gameObject();
    if (!go) return;

    Vec3& rot = go->transform().rotation; // stored in radians
    rot.x += angularVelocityEuler.x * kDegToRad * deltaTime;
    rot.y += angularVelocityEuler.y * kDegToRad * deltaTime;
    rot.z += angularVelocityEuler.z * kDegToRad * deltaTime;
}

} // namespace scene
