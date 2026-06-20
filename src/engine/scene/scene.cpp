#include "scene/scene.h"

GameObject* Scene::createObject(const std::string& name)
{
    auto obj = std::make_unique<GameObject>(allocateObjectId(), name);
    GameObject* ptr = obj.get();
    objects_.push_back(std::move(obj));
    return ptr;
}

Camera* Scene::createCamera(const Vec3& position)
{
    auto cam = std::make_unique<Camera>(allocateObjectId(), "Camera", position);
    Camera* ptr = cam.get();
    camera_ = ptr;
    objects_.push_back(std::move(cam));
    return ptr;
}

GameObject* Scene::findObjectById(uint64_t id) const
{
    for (const auto& obj : objects_) {
        if (obj->id().value == id) {
            return obj.get();
        }
    }
    return nullptr;
}

ObjectId Scene::allocateObjectId()
{
    return ObjectId{nextObjectId_++};
}

void Scene::buildRenderCommands(RenderContext& ctx) const
{
    ctx.drawGround(camera_->transform().position);

    for (const auto& obj : objects_) {
        if (!obj->active()) continue;
        ctx.drawCube(obj->transform().localMatrix(), obj->color);
    }
}
