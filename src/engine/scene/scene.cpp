#include "scene/scene.h"

GameObject* Scene::createObject(const std::string& name)
{
    auto obj = std::make_unique<GameObject>(name);
    GameObject* ptr = obj.get();
    objects_.push_back(std::move(obj));
    return ptr;
}

void Scene::buildRenderCommands(RenderContext& ctx) const
{
    ctx.drawGround(camera_.position);

    for (const auto& obj : objects_) {
        if (!obj->active()) continue;
        ctx.drawCube(obj->transform().localMatrix(), obj->color);
    }
}
