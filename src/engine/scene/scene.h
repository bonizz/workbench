#pragma once

#include "scene/game_object.h"
#include "scene/camera.h"
#include "renderer/render_context.h"

#include <memory>
#include <vector>

class Scene
{
public:
    GameObject* createObject(const std::string& name);

    Camera& camera() { return camera_; }
    const Camera& camera() const { return camera_; }

    void buildRenderCommands(RenderContext& ctx) const;

private:
    Camera camera_;
    std::vector<std::unique_ptr<GameObject>> objects_;
};
