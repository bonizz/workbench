#include "scene/scene.h"

#include "renderer/render_command.h"
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

bool Scene::isDescendant(const GameObject* possibleChild, const GameObject* possibleAncestor) const
{
    // Returns true if possibleAncestor is reachable downwards from possibleChild
    // (i.e. possibleAncestor is in possibleChild's subtree). Used to reject
    // cycles: if the proposed parent is already under the proposed child,
    // parenting would create a loop.
    if (!possibleChild || !possibleAncestor) return false;
    for (GameObject* child : possibleChild->children_) {
        if (child == possibleAncestor) return true;
        if (isDescendant(child, possibleAncestor)) return true;
    }
    return false;
}

bool Scene::setParent(GameObject* child, GameObject* parent)
{
    if (!child || child == camera_) return false;
    if (parent == camera_) return false;
    if (child == parent) return false;

    // Reject cycles: if `parent` is already in `child`'s subtree, attaching
    // child under parent would create a loop.
    if (parent && isDescendant(child, parent)) return false;

    // Idempotent: nothing to do if already attached to this parent.
    if (child->parent_ == parent) {
        updateWorldTransforms();
        return true;
    }

    // Detach from current parent's children_ vector.
    if (GameObject* oldParent = child->parent_) {
        auto it = std::find(oldParent->children_.begin(), oldParent->children_.end(), child);
        if (it != oldParent->children_.end()) {
            oldParent->children_.erase(it);
        }
    }

    // Attach to new parent (or leave as a root).
    child->parent_ = parent;
    if (parent) {
        parent->children_.push_back(child);
    }

    updateWorldTransforms();
    return true;
}

void Scene::updateWorldTransforms()
{
    for (const auto& obj : objects_) {
        if (obj->parent_ == nullptr) {
            recomputeWorldRecursive(*obj, identity());
        }
    }
}

void Scene::recomputeWorldRecursive(GameObject& obj, const Mat4& parentWorld)
{
    Mat4 world = multiply(parentWorld, obj.transform_.localMatrix());
    obj.transform_.worldMatrix_ = world;
    for (GameObject* child : obj.children_) {
        recomputeWorldRecursive(*child, world);
    }
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
    // Components mutate local transform fields; refresh the derived world
    // matrices so a subsequent read (e.g. assert.world_position) is correct.
    updateWorldTransforms();
}

namespace {

ShapeType toRenderShape(scene::MeshShape shape)
{
    switch (shape) {
        case scene::MeshShape::Cube:   return ShapeType::Cube;
        case scene::MeshShape::Sphere: return ShapeType::Sphere;
        case scene::MeshShape::Plane:  return ShapeType::Plane;
    }
    return ShapeType::Cube;
}

} // namespace

void Scene::buildRenderCommands(RenderContext& ctx)
{
    // Refresh first so editor direct-edits to local fields are reflected.
    updateWorldTransforms();

    ctx.drawGround(camera_->transform().position);

    for (const auto& obj : objects_) {
        if (!obj->active()) continue;
        if (MeshRenderer* mesh = obj->getComponent<MeshRenderer>()) {
            ctx.drawShape(toRenderShape(mesh->shape), obj->transform().worldMatrix(), mesh->color);
        }
    }
}

bool Scene::deleteObject(GameObject* obj)
{
    if (!obj || obj == camera_) {
        return false;
    }

    // Detach from parent's children_ so no dangling parent pointer survives.
    if (GameObject* parent = obj->parent_) {
        auto it = std::find(parent->children_.begin(), parent->children_.end(), obj);
        if (it != parent->children_.end()) {
            parent->children_.erase(it);
        }
    }

    // Collect obj + all descendants. Order is irrelevant to deletion.
    std::vector<GameObject*> subtree;
    std::vector<GameObject*> workStack = {obj};
    while (!workStack.empty()) {
        GameObject* cur = workStack.back();
        workStack.pop_back();
        subtree.push_back(cur);
        for (GameObject* child : cur->children()) {
            workStack.push_back(child);
        }
    }

    // Erase every doomed object from objects_.
    objects_.erase(
        std::remove_if(objects_.begin(), objects_.end(),
            [&](const std::unique_ptr<GameObject>& o) {
                return std::find(subtree.begin(), subtree.end(), o.get()) != subtree.end();
            }),
        objects_.end());
    return true;
}

GameObject* Scene::duplicateObject(GameObject* obj)
{
    if (!obj || obj == camera_) {
        return nullptr;
    }

    // Root copy: gets the " Copy" suffix, parent is null, local transform cloned.
    GameObject* copy = createObject(obj->name() + " Copy");
    copy->transform().position = obj->transform().position;
    copy->transform().rotation = obj->transform().rotation;
    copy->transform().scale = obj->transform().scale;
    for (const auto& comp : obj->components()) {
        copy->addComponent(comp->clone());
    }

    // Recursively duplicate the subtree (child names preserved).
    for (GameObject* child : obj->children()) {
        duplicateRecursive(child, copy);
    }
    return copy;
}

GameObject* Scene::duplicateRecursive(const GameObject* src, GameObject* newParent)
{
    GameObject* copy = createObject(src->name());
    copy->transform().position = src->transform().position;
    copy->transform().rotation = src->transform().rotation;
    copy->transform().scale = src->transform().scale;
    for (const auto& comp : src->components()) {
        copy->addComponent(comp->clone());
    }
    // copy is fresh and newParent is a real object; setParent always succeeds.
    setParent(copy, newParent);

    for (GameObject* child : src->children()) {
        duplicateRecursive(child, copy);
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
