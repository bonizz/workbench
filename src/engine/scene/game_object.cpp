#include "scene/game_object.h"

using scene::Component;

GameObject::GameObject(ObjectId id, std::string name)
    : id_(id)
    , name_(std::move(name))
{
}

Component* GameObject::addComponent(std::unique_ptr<Component> component)
{
    if (!component) {
        return nullptr;
    }
    component->setGameObject(this);
    Component* ptr = component.get();
    components_.push_back(std::move(component));
    return ptr;
}
