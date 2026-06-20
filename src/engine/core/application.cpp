#include "core/application.h"
#include "core/math.h"
#include "platform/window.h"
#include "renderer/metal_renderer.h"
#include "renderer/render_context.h"
#include "editor/editor.h"
#include "scene/scene.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"

using scene::MeshRenderer;

#include "imgui/imgui.h"

#include <cmath>

Application::Application()
{
}

Application::~Application()
{
    shutdown();
}

bool Application::init()
{
    window_ = std::make_unique<Window>(*this, "Workbench", 800, 600);
    renderer_ = std::make_unique<MetalRenderer>(window_->metalLayer());
    editor_ = std::make_unique<Editor>();

    if (!editor_->init(window_->nativeView(), renderer_->device())) {
        return false;
    }

    scene_ = std::make_unique<Scene>();
    scene_->createCamera({0.0f, 3.0f, 5.0f});
    scene_->camera().setActive(false);
    scene_->camera().setAspect(renderer_->aspectRatio());

    GameObject* cube = scene_->createObject("Cube");
    cube->transform().position = {0.0f, 0.5f, 0.0f};
    cube->transform().scale = {0.5f, 0.5f, 0.5f};
    auto mesh = std::make_unique<MeshRenderer>();
    mesh->color = {0.95f, 0.55f, 0.20f, 1.0f};
    cube->addComponent(std::move(mesh));

    onResize(window_->width(), window_->height(), window_->backingScale());
    return true;
}

void Application::run()
{
    window_->run();
}

void Application::shutdown()
{
    editor_.reset();
    renderer_.reset();
    window_.reset();
    scene_.reset();
}

void Application::onResize(float width, float height, float scale)
{
    if (renderer_) {
        renderer_->resize(width, height, scale);
    }
    if (scene_) {
        scene_->camera().setAspect(renderer_ ? renderer_->aspectRatio() : (width / height));
    }
}

void Application::onUpdate(float deltaTime)
{
    time_.update(deltaTime);

    InputState input;
    input.forward = keyW_;
    input.backward = keyS_;
    input.left = keyA_;
    input.right = keyD_;
    input.mouseDeltaX = mouseDeltaX_;
    input.mouseDeltaY = mouseDeltaY_;
    input.scrollDelta = scrollDelta_;

    scene_->camera().update(time_.deltaTime(), input);

    mouseDeltaX_ = 0.0f;
    mouseDeltaY_ = 0.0f;
    scrollDelta_ = 0.0f;

    if (deltaTime > 0.0f) {
        fps_ = 1.0f / deltaTime;
        frameTimeMs_ = deltaTime * 1000.0f;
    }
}

void Application::onRender()
{
    ++frame_;

    editor_->beginFrame(window_->nativeView());
    editor_->drawUI(*scene_, frame_, fps_, frameTimeMs_, lastRenderCommandCount_);
    editor_->endFrame();

    RenderContext ctx;
    ctx.setCamera(scene_->camera().viewMatrix(), scene_->camera().projectionMatrix());
    scene_->buildRenderCommands(ctx);
    lastRenderCommandCount_ = ctx.commands().size();

    renderer_->draw(ctx, clearColor_, [this](void* commandBuffer, void* renderEncoder, void* renderPassDescriptor) {
        editor_->render(commandBuffer, renderEncoder, renderPassDescriptor);
    });
}

void Application::onKeyEvent(int keyCode, bool down)
{
    if (ImGui::GetIO().WantCaptureKeyboard) {
        return;
    }

    switch (keyCode) {
        case 'w': case 'W': keyW_ = down; break;
        case 'a': case 'A': keyA_ = down; break;
        case 's': case 'S': keyS_ = down; break;
        case 'd': case 'D': keyD_ = down; break;
        default: break;
    }
}

void Application::onMouseDrag(float deltaX, float deltaY)
{
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    mouseDeltaX_ += deltaX;
    mouseDeltaY_ += deltaY;
}

void Application::onScroll(float delta)
{
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    scrollDelta_ += delta;
}
