#pragma once

#include "core/object_id.h"
#include "scene/component.h"
#include "scene/transform.h"

#include <memory>
#include <string>
#include <vector>

namespace scene { class MeshRenderer; }

class GameObject
{
public:
    GameObject(ObjectId id, std::string name);
    virtual ~GameObject() = default;

    ObjectId id() const { return id_; }

    const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    Transform& transform() { return transform_; }
    const Transform& transform() const { return transform_; }

    bool active() const { return active_; }
    void setActive(bool active) { active_ = active; }

    // Hierarchy. Non-owning pointers; Scene is the sole mutator.
    // See docs/plans/0.6_transform_hierarchy.md §3.
    GameObject* parent() const { return parent_; }
    const std::vector<GameObject*>& children() const { return children_; }

    // Component ownership.
    scene::Component* addComponent(std::unique_ptr<scene::Component> component);

    template<typename T>
    T* getComponent();

    template<typename T>
    const T* getComponent() const;

    template<typename T>
    bool hasComponent() const;

    const std::vector<std::unique_ptr<scene::Component>>& components() const { return components_; }

private:
    ObjectId id_;
    std::string name_;
    Transform transform_;
    bool active_ = true;
    std::vector<std::unique_ptr<scene::Component>> components_;

    friend class Scene;
    GameObject* parent_ = nullptr;
    std::vector<GameObject*> children_; // insertion order; deterministic
};

// Inline template definitions must be in the header because the type is not known here.
template<typename T>
T* GameObject::getComponent()
{
    for (const auto& comp : components_) {
        if (T* typed = dynamic_cast<T*>(comp.get())) {
            return typed;
        }
    }
    return nullptr;
}

template<typename T>
const T* GameObject::getComponent() const
{
    for (const auto& comp : components_) {
        if (const T* typed = dynamic_cast<const T*>(comp.get())) {
            return typed;
        }
    }
    return nullptr;
}

template<typename T>
bool GameObject::hasComponent() const
{
    return getComponent<T>() != nullptr;
}

