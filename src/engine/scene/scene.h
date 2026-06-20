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

    void buildRenderCommands(RenderContext& ctx) const;

    bool deleteObject(GameObject* obj);
    GameObject* duplicateObject(GameObject* obj);
    void clearObjects();

    bool isCamera(const GameObject* obj) const { return obj == camera_; }

    const std::string& loadedScenePath() const { return loadedScenePath_; }
    void setLoadedScenePath(std::string path) { loadedScenePath_ = std::move(path); }

private:
    ObjectId allocateObjectId();

    uint64_t nextObjectId_ = 1;
    Camera* camera_ = nullptr;
    std::vector<std::unique_ptr<GameObject>> objects_;
    std::string loadedScenePath_;
};
