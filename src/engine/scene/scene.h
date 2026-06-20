#pragma once

#include "core/object_id.h"
#include "scene/camera.h"
#include "scene/game_object.h"
#include "renderer/render_context.h"

#include <memory>
#include <vector>

class Scene
{
public:
    GameObject* createObject(const std::string& name);
    Camera* createCamera(const Vec3& position);

    GameObject* findObjectById(uint64_t id) const;

    Camera& camera() { return *camera_; }
    const Camera& camera() const { return *camera_; }

    const std::vector<std::unique_ptr<GameObject>>& objects() const { return objects_; }

    // Hierarchy. Scene is the sole mutator of parent_/children_ pointers.
    // Returns false (and leaves the hierarchy unchanged) on: null child, child
    // is the camera, parent is the camera, child == parent, or parenting would
    // create a cycle. parent == nullptr detaches (makes child a root).
    // Idempotent: setting the current parent again is a success no-op.
    // Always leaves world transforms fresh.
    bool setParent(GameObject* child, GameObject* parent);

    // Recomputes every object's worldMatrix_ via DFS from roots.
    // Called by buildRenderCommands, end of update, setParent, DebugState::build,
    // and world-reading commands (see 0.6 plan §4 freshness contract).
    void updateWorldTransforms();

    // Advances all components. Iterates objects in creation/ObjectId order,
    // skipping inactive ones, and per object iterates components in add order.
    // Each component gets onStart() once (lazily, before its first onUpdate)
    // then onUpdate(dt). Camera is unaffected (it has no components).
    // World transforms are refreshed at the end so a subsequent read
    // (e.g. assert.world_position) is deterministic.
    void update(float deltaTime);

    // Uses each object's worldMatrix() (parent * local). Refreshes world
    // transforms first so editor direct-edits are reflected immediately.
    void buildRenderCommands(RenderContext& ctx);

    // Cascades: deletes the object and its entire subtree from objects_.
    bool deleteObject(GameObject* obj);

    // Cascades: duplicates the object's subtree (local transform + components +
    // child names). The root copy gets a " Copy" name suffix.
    GameObject* duplicateObject(GameObject* obj);

    void clearObjects();

    bool isCamera(const GameObject* obj) const { return obj == camera_; }

    const std::string& loadedScenePath() const { return loadedScenePath_; }
    void setLoadedScenePath(std::string path) { loadedScenePath_ = std::move(path); }

private:
    ObjectId allocateObjectId();
    bool isDescendant(const GameObject* possibleChild, const GameObject* possibleAncestor) const;
    void recomputeWorldRecursive(GameObject& obj, const Mat4& parentWorld);
    GameObject* duplicateRecursive(const GameObject* src, GameObject* newParent);

    uint64_t nextObjectId_ = 1;
    Camera* camera_ = nullptr;
    std::vector<std::unique_ptr<GameObject>> objects_;
    std::string loadedScenePath_;
};
