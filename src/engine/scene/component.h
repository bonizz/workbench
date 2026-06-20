#pragma once

#include <memory>

class GameObject;

namespace scene {

// NOTE: This type is inside the scene namespace because macOS system headers
// define a global typedef named Component. Keeping it namespaced avoids the
// collision without renaming the class.
class Component
{
public:
    virtual ~Component() = default;

    // Used for UI, debug output, and serialization dispatch.
    virtual const char* typeName() const = 0;

    // Explicit copy helper so GameObject::duplicateObject does not need a type switch.
    virtual std::unique_ptr<Component> clone() const = 0;

    // Lifecycle hooks. Default no-ops so existing components are unaffected.
    // onStart is called once, before the first onUpdate, the first time the
    // component participates in Scene::update. onUpdate is called every update
    // thereafter. See docs/components.md for the exact update order.
    virtual void onStart() {}
    virtual void onUpdate(float /*deltaTime*/) {}

    // Drives the lazy one-shot onStart transition. Encapsulates the started
    // flag so callers do not need to touch private state. Returns true and
    // fires onStart() the first time it is called; no-op thereafter.
    void startIfNeeded();

    GameObject* gameObject() const { return gameObject_; }
    void setGameObject(GameObject* gameObject) { gameObject_ = gameObject; }

private:
    GameObject* gameObject_ = nullptr;
    bool started_ = false;
};

} // namespace scene
