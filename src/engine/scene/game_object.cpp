#include "scene/game_object.h"

GameObject::GameObject(ObjectId id, std::string name)
    : id_(id)
    , name_(std::move(name))
{
}
