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

    GameObject* gameObject() const { return gameObject_; }
    void setGameObject(GameObject* gameObject) { gameObject_ = gameObject; }

private:
    GameObject* gameObject_ = nullptr;
};

} // namespace scene
