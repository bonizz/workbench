#include "core/application.h"
#include "core/math.h"
#include "platform/window.h"
#include "renderer/metal_renderer.h"
#include "renderer/render_context.h"
#include "editor/editor.h"
#include "scene/scene.h"
#include "scene/game_object.h"

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
    scene_->camera().position = {0.0f, 3.0f, 5.0f};
    scene_->camera().yaw = 0.0f;
    scene_->camera().pitch = -0.35f;

    GameObject* cube = scene_->createObject("Cube");
    cube->transform().position = {0.0f, 0.5f, 0.0f};
    cube->transform().scale = {0.5f, 0.5f, 0.5f};
    cube->color = {0.95f, 0.55f, 0.20f, 1.0f};

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
}

void Application::onUpdate(float deltaTime)
{
    time_.update(deltaTime);
    updateCamera(time_.deltaTime());

    if (deltaTime > 0.0f) {
        fps_ = 1.0f / deltaTime;
        frameTimeMs_ = deltaTime * 1000.0f;
    }
}

void Application::onRender()
{
    editor_->beginFrame(window_->nativeView());
    editor_->drawPanels(*scene_, fps_, frameTimeMs_);
    editor_->endFrame();

    RenderContext ctx;
    Mat4 view = scene_->camera().viewMatrix();
    Mat4 projection = perspective(60.0f * 3.14159f / 180.0f, renderer_->aspectRatio(), 0.1f, 100.0f);
    ctx.setCamera(view, projection);
    scene_->buildRenderCommands(ctx);

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

    Camera& cam = scene_->camera();
    cam.yaw += deltaX * mouseSensitivity_;
    cam.pitch += deltaY * mouseSensitivity_;

    const float maxPitch = 85.0f * 3.14159f / 180.0f;
    if (cam.pitch > maxPitch) cam.pitch = maxPitch;
    if (cam.pitch < -maxPitch) cam.pitch = -maxPitch;
}

void Application::onScroll(float delta)
{
    moveSpeed_ += delta;
    if (moveSpeed_ < 1.0f) moveSpeed_ = 1.0f;
    if (moveSpeed_ > 50.0f) moveSpeed_ = 50.0f;
}

void Application::updateCamera(float deltaTime)
{
    if (ImGui::GetIO().WantCaptureKeyboard) {
        return;
    }

    Camera& cam = scene_->camera();
    Vec3 f = cam.forward();
    Vec3 r = cam.right();

    float forwardInput = (keyW_ ? 1.0f : 0.0f) - (keyS_ ? 1.0f : 0.0f);
    float rightInput   = (keyD_ ? 1.0f : 0.0f) - (keyA_ ? 1.0f : 0.0f);

    float speed = moveSpeed_ * deltaTime;
    cam.position.x += (f.x * forwardInput + r.x * rightInput) * speed;
    cam.position.y += (f.y * forwardInput + r.y * rightInput) * speed;
    cam.position.z += (f.z * forwardInput + r.z * rightInput) * speed;
}
