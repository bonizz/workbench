#include "scene/scene.h"

#include "scene/mesh_renderer.h"

#include <algorithm>

using scene::MeshRenderer;

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

void Scene::update(float deltaTime)
{
    for (const auto& obj : objects_) {
        if (!obj->active()) continue;
        for (const auto& comp : obj->components()) {
            comp->startIfNeeded();
            comp->onUpdate(deltaTime);
        }
    }
}

void Scene::buildRenderCommands(RenderContext& ctx) const
{
    ctx.drawGround(camera_->transform().position);

    for (const auto& obj : objects_) {
        if (!obj->active()) continue;
        if (MeshRenderer* mesh = obj->getComponent<MeshRenderer>()) {
            ctx.drawCube(obj->transform().localMatrix(), mesh->color);
        }
    }
}

bool Scene::deleteObject(GameObject* obj)
{
    if (!obj || obj == camera_) {
        return false;
    }

    auto it = std::find_if(objects_.begin(), objects_.end(),
                           [obj](const std::unique_ptr<GameObject>& o) {
                               return o.get() == obj;
                           });
    if (it == objects_.end()) {
        return false;
    }

    objects_.erase(it);
    return true;
}

GameObject* Scene::duplicateObject(GameObject* obj)
{
    if (!obj || obj == camera_) {
        return nullptr;
    }

    GameObject* copy = createObject(obj->name() + " Copy");
    copy->transform() = obj->transform();
    for (const auto& comp : obj->components()) {
        copy->addComponent(comp->clone());
    }
    return copy;
}

void Scene::clearObjects()
{
    std::unique_ptr<GameObject> camera;
    for (auto& obj : objects_) {
        if (obj.get() == camera_) {
            camera = std::move(obj);
        }
    }

    objects_.clear();
    if (camera) {
        objects_.push_back(std::move(camera));
    }
}
