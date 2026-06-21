#include "core/application.h"
#include "agent/command.h"
#include "agent/test_suite.h"
#include "core/math.h"
#include "core/picking.h"
#include "core/settings.h"
#include "platform/window.h"
#include "renderer/metal_renderer.h"
#include "renderer/render_context.h"
#include "editor/editor.h"
#include "scene/scene.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"

using scene::MeshRenderer;

#include "imgui/imgui.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <thread>

Application::Application()
{
}

Application::~Application()
{
    shutdown();
}

bool Application::init()
{
    // Simulation advances per frame only when no automation is requested.
    // In automation mode (--run-script/--bundle/--run-tests) the simulation
    // advances exclusively via `sim.step` for determinism. Decide this early
    // so camera persistence only affects interactive sessions.
    liveSimulation_ = cliOptions_.runScript.empty()
                   && cliOptions_.bundleName.empty()
                   && !cliOptions_.runTests;

    int windowWidth = 800;
    int windowHeight = 600;
    if (Settings::loadWindowSize(windowWidth, windowHeight)) {
        if (windowWidth < 640) windowWidth = 640;
        if (windowHeight < 480) windowHeight = 480;
    }

    window_ = std::make_unique<Window>(*this, "Workbench", windowWidth, windowHeight);

    int windowX = 0;
    int windowY = 0;
    if (Settings::loadWindowPosition(windowX, windowY)) {
        window_->setPosition(windowX, windowY);
    }

    renderer_ = std::make_unique<MetalRenderer>(window_->metalLayer());
    editor_ = std::make_unique<Editor>();

    if (!editor_->init(window_->nativeView(), renderer_->device())) {
        return false;
    }
    editor_->setRenderer(renderer_.get());
    editor_->setLightSettings(&lightSettings_);
    editor_->setSkySettings(&skySettings_);

    lightSettings_.direction = {-0.5f, -1.0f, -0.75f};
    lightSettings_.ambient = 0.15f;
    lightSettings_.diffuse = 1.0f;

    // Restore saved light/sky tweaks, falling back to the defaults above.
    Settings::loadLighting(lightSettings_, skySettings_);

    scene_ = std::make_unique<Scene>();
    scene_->createCamera({0.0f, 3.0f, 5.0f});
    scene_->camera().setActive(false);
    scene_->camera().setAspect(renderer_->aspectRatio());

    if (liveSimulation_) {
        Vec3 cameraPos = scene_->camera().transform().position;
        Vec3 cameraRot = scene_->camera().transform().rotation;
        float cameraSpeed = scene_->camera().moveSpeed();
        if (Settings::loadCamera(cameraPos, cameraRot, cameraSpeed)) {
            scene_->camera().transform().position = cameraPos;
            scene_->camera().transform().rotation = cameraRot;
            scene_->camera().setMoveSpeed(cameraSpeed);
        }
    }

    GameObject* cube = scene_->createObject("Cube");
    cube->transform().position = {0.0f, 0.5f, 0.0f};
    cube->transform().scale = {0.5f, 0.5f, 0.5f};
    auto mesh = std::make_unique<MeshRenderer>();
    mesh->color = {0.95f, 0.55f, 0.20f, 1.0f};
    cube->addComponent(std::move(mesh));

    onResize(window_->width(), window_->height(), window_->backingScale());

    return true;
}

int Application::run()
{
    window_->run();
    shutdown();
    return exitCode();
}

void Application::setCliOptions(const CliOptions& options)
{
    cliOptions_ = options;
}

int Application::exitCode() const
{
    return automationFailed_ ? 1 : 0;
}

void Application::saveWindowSize()
{
    if (window_) {
        Settings::saveWindowSize(static_cast<int>(window_->width()), static_cast<int>(window_->height()));
    }
}

void Application::saveSettings()
{
    if (window_) {
        Settings::saveWindowSize(static_cast<int>(window_->width()), static_cast<int>(window_->height()));
        Settings::saveWindowPosition(static_cast<int>(window_->x()), static_cast<int>(window_->y()));
    }
    if (editor_) {
        editor_->saveSettings();
        editor_->saveLayout();
    }
    Settings::saveLighting(lightSettings_, skySettings_);
    if (liveSimulation_ && scene_) {
        const Camera& cam = scene_->camera();
        Settings::saveCamera(cam.transform().position, cam.transform().rotation, cam.moveSpeed());
    }
}

void Application::shutdown()
{
    saveSettings();

    editor_.reset();
    renderer_.reset();
    window_.reset();
    scene_.reset();
}

void Application::recreateScene()
{
    scene_ = std::make_unique<Scene>();
    scene_->createCamera({0.0f, 3.0f, 5.0f});
    scene_->camera().setActive(false);
    if (renderer_) {
        scene_->camera().setAspect(renderer_->aspectRatio());
    }
    if (editor_) {
        editor_->setSelected(nullptr);
    }
}

void Application::onResize(float width, float height, float scale)
{
    if (renderer_) {
        renderer_->resize(width, height, scale);
    }
    if (scene_) {
        scene_->camera().setAspect(renderer_ ? renderer_->aspectRatio() : (width / height));
    }
    Settings::saveWindowSize(static_cast<int>(width), static_cast<int>(height));
}

void Application::waitForPendingScreenshot()
{
    if (pendingScreenshotPath_.empty()) {
        return;
    }

    using namespace std::chrono;
    auto start = steady_clock::now();
    while (!std::filesystem::exists(pendingScreenshotPath_)) {
        auto elapsed = duration_cast<seconds>(steady_clock::now() - start).count();
        if (elapsed > 5) {
            std::fprintf(stderr, "Timed out waiting for screenshot: %s\n", pendingScreenshotPath_.c_str());
            break;
        }
        std::this_thread::sleep_for(50ms);
    }
}

void Application::runTestSuite()
{
    auto tests = TestSuite::discoverTests("assets/tests");
    TestSuite::Summary summary;

    for (const auto& path : tests) {
        recreateScene();
        lastAssertionFailure_.clear();

        GameObject* selected = nullptr;
        AgentCommandContext ctx{*scene_, selected, frame_, fps_, frameTimeMs_,
                                lastRenderCommandCount_, {}, renderer_.get(),
                                {}, {}, &lastAssertionFailure_};

        TestSuite::Result result = TestSuite::runTestFile(*scene_, ctx, path);

        if (!result.passed) {
            TestSuite::createFailureBundle(result.name, ctx, result.bundlePath);
        }

        std::printf("%s %s\n", result.passed ? "PASS" : "FAIL", result.name.c_str());
        if (!result.passed && !result.failure.empty()) {
            std::printf("      %s\n", result.failure.c_str());
        }

        summary.results.push_back(std::move(result));
    }

    std::printf("\nSummary:\n");
    std::printf("Passed: %zu\n", summary.passedCount());
    std::printf("Failed: %zu\n", summary.failedCount());

    if (!summary.allPassed()) {
        automationFailed_ = true;
    }
}

void Application::runAutomation()
{
    GameObject* selected = nullptr;
    AgentCommandContext ctx{*scene_, selected, frame_, fps_, frameTimeMs_,
                            lastRenderCommandCount_, {}, renderer_.get(),
                            {}, {}, &lastAssertionFailure_};

    auto runAutomationCommand = [&](const std::string& command) {
        AgentCommandResult result = executeCommand(command, ctx);
        if (!result.success) {
            std::fprintf(stderr, "Automation failed: %s\n", result.output.c_str());
            automationFailed_ = true;
        } else {
            std::printf("%s\n", result.output.c_str());
        }
        return result.success;
    };

    if (!cliOptions_.runScript.empty()) {
        runAutomationCommand("script.run " + cliOptions_.runScript);
    }

    if (!cliOptions_.bundleName.empty()) {
        if (runAutomationCommand("debug.bundle " + cliOptions_.bundleName)) {
            pendingScreenshotPath_ = ctx.lastCapturePath;
        }
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
    input.up = keyE_;
    input.down = keyQ_;
    input.mouseDeltaX = mouseDeltaX_;
    input.mouseDeltaY = mouseDeltaY_;
    input.scrollDelta = scrollDelta_;

    scene_->camera().update(time_.deltaTime(), input);

    if (liveSimulation_) {
        scene_->update(time_.deltaTime());
    }

    mouseDeltaX_ = 0.0f;
    mouseDeltaY_ = 0.0f;
    scrollDelta_ = 0.0f;

    if (deltaTime > 0.0f) {
        fps_ = 1.0f / deltaTime;
        frameTimeMs_ = deltaTime * 1000.0f;
    }

    switch (automationState_) {
        case AutomationState::Pending: {
            bool hasWork = !cliOptions_.runScript.empty() || !cliOptions_.bundleName.empty() || cliOptions_.runTests;
            if (!cliOptions_.runScript.empty() || !cliOptions_.bundleName.empty()) {
                runAutomation();
            }
            if (cliOptions_.runTests) {
                runTestSuite();
            }

            if (hasWork && cliOptions_.autoExit) {
                automationWaitFrames_ = std::max(0, cliOptions_.extraFrames);
                automationState_ = AutomationState::WaitingExtraFrames;
            } else {
                automationState_ = AutomationState::Done;
            }
            break;
        }
        case AutomationState::WaitingExtraFrames:
            if (--automationWaitFrames_ <= 0) {
                automationState_ = AutomationState::Done;
                waitForPendingScreenshot();
                if (window_) {
                    window_->terminate();
                }
            }
            break;
        default:
            break;
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
    Vec3 dir = {lightSettings_.direction.x, lightSettings_.direction.y, lightSettings_.direction.z};
    ctx.setLight(dir, lightSettings_.ambient, lightSettings_.diffuse);
    ctx.setSky(skySettings_);
    scene_->buildRenderCommands(ctx, editor_->selected());
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
        case 'q': case 'Q': keyQ_ = down; break;
        case 'e': case 'E': keyE_ = down; break;
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

Ray Application::rayFromPixel(float x, float y) const
{
    float width = window_->width();
    float height = window_->height();
    float ndcX = (x / width) * 2.0f - 1.0f;
    float ndcY = (y / height) * 2.0f - 1.0f;

    return makeCameraRay(scene_->camera().transform().position,
                         scene_->camera().viewMatrix(),
                         renderer_->aspectRatio(),
                         60.0f * kDegToRad,
                         ndcX,
                         ndcY);
}

void Application::onMouseButton(int button, bool down, float x, float y)
{
    if (button != 0) {
        return;
    }

    // Mouse up always ends any gizmo drag, even over an ImGui panel, so the
    // drag state never lingers past a release.
    if (!down) {
        gizmoDragging_ = false;
        gizmoDragTarget_ = nullptr;
        return;
    }

    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    float width = window_->width();
    float height = window_->height();
    if (width <= 0.0f || height <= 0.0f) {
        return;
    }

    Ray ray = rayFromPixel(x, y);

    // If a selected MeshRenderer's gizmo handle is clicked, begin a drag instead
    // of reselecting. The handle is a sphere at the object's world origin.
    GameObject* selected = const_cast<GameObject*>(editor_->selected());
    if (selected && selected->getComponent<MeshRenderer>()) {
        scene_->updateWorldTransforms();
        simd::float4 wp = selected->transform().worldMatrix().columns[3];
        Vec3 center = {wp.x, wp.y, wp.z};
        Vec3 local = {ray.origin.x - center.x, ray.origin.y - center.y, ray.origin.z - center.z};
        float t = 0.0f;
        if (intersectRaySphere(local, ray.direction, kGizmoHandleRadius, t)) {
            Vec3 planeHit;
            if (intersectRayHorizontalPlane(ray.origin, ray.direction, center.y, planeHit)) {
                gizmoDragging_ = true;
                gizmoDragTarget_ = selected;
                gizmoDragPlaneY_ = center.y;
                gizmoDragOffset_ = {center.x - planeHit.x, 0.0f, center.z - planeHit.z};
                return;
            }
        }
    }

    GameObject* hit = scene_->pickObject(ray.origin, ray.direction);
    editor_->setSelected(hit);
}

void Application::onLeftMouseMove(float x, float y)
{
    if (!gizmoDragging_ || gizmoDragTarget_ == nullptr) {
        return;
    }
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    float width = window_->width();
    float height = window_->height();
    if (width <= 0.0f || height <= 0.0f) {
        return;
    }

    Ray ray = rayFromPixel(x, y);
    Vec3 hit;
    if (!intersectRayHorizontalPlane(ray.origin, ray.direction, gizmoDragPlaneY_, hit)) {
        return;
    }

    // Update X/Z only; Y is preserved by planeDragPosition. Edits the local
    // position (matches the inspector); for an unparented object local == world.
    gizmoDragTarget_->transform().position =
        planeDragPosition(hit, gizmoDragOffset_, gizmoDragPlaneY_);
}
