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
#include "scene/scene_serializer.h"
#include "scene/scene_io.h"

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
    editor_->setApplication(this);
    editor_->setRenderer(renderer_.get());

    scene_ = std::make_unique<Scene>();
    scene_->createCamera({0.0f, 3.0f, 5.0f});
    scene_->camera().setActive(false);

    editor_->setLightSettings(&scene_->environment().light);
    editor_->setSkySettings(&scene_->environment().sky);

    // Restore the last edited scene if possible, otherwise the shipped default.
    bool loadedScene = false;
    std::string lastScenePath;
    if (Settings::loadLastScene(lastScenePath) && std::filesystem::exists(lastScenePath)) {
        std::string err;
        std::string warn;
        if (SceneSerializer::load(*scene_, lastScenePath, err, &warn)) {
            scene_->setLoadedScenePath(lastScenePath);
            Settings::addRecentScene(lastScenePath);
            loadedScene = true;
        }
    }
    if (!loadedScene && std::filesystem::exists("assets/scenes/default.scene.json")) {
        std::string err;
        std::string warn;
        if (SceneSerializer::load(*scene_, "assets/scenes/default.scene.json", err, &warn)) {
            scene_->setLoadedScenePath("assets/scenes/default.scene.json");
            loadedScene = true;
        }
    }

    // In automation mode keep the camera at its deterministic default even if
    // the loaded scene stored a different viewpoint.
    if (!liveSimulation_) {
        scene_->camera().reset();
    }

    scene_->camera().setAspect(renderer_->aspectRatio());

    if (!loadedScene) {
        // Built-in fallback: the historical hard-coded cube.
        GameObject* cube = scene_->createObject("Cube");
        cube->transform().position = {0.0f, 0.5f, 0.0f};
        cube->transform().scale = {0.5f, 0.5f, 0.5f};
        auto mesh = std::make_unique<MeshRenderer>();
        mesh->color = {0.95f, 0.55f, 0.20f, 1.0f};
        cube->addComponent(std::move(mesh));
    }

    updateWindowTitle();

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
    if (liveSimulation_ && scene_ && !scene_->loadedScenePath().empty()) {
        Settings::saveLastScene(scene_->loadedScenePath());
    }
}

void Application::updateWindowTitle()
{
    if (!window_) {
        return;
    }

    std::string title = "Workbench";
    std::string path = scene_ ? scene_->loadedScenePath() : std::string();
    if (!path.empty()) {
        size_t pos = path.find_last_of("/\\");
        std::string name = (pos == std::string::npos) ? path : path.substr(pos + 1);
        title += " - " + name;
    } else {
        title += " - Untitled";
    }
    if (sceneDirty_) {
        title += " *";
    }
    window_->setTitle(title.c_str());
}

void Application::requestQuit()
{
    if (window_) {
        window_->terminate();
    }
}

void Application::newScene()
{
    recreateScene();

    if (scene_) {
        bool loadedDefault = false;
        if (std::filesystem::exists("assets/scenes/default.scene.json")) {
            std::string error;
            std::string warning;
            loadedDefault = SceneSerializer::load(*scene_, "assets/scenes/default.scene.json", error, &warning);
        }

        if (!loadedDefault) {
            // Built-in fallback: the historical hard-coded cube.
            GameObject* cube = scene_->createObject("Cube");
            cube->transform().position = {0.0f, 0.5f, 0.0f};
            cube->transform().scale = {0.5f, 0.5f, 0.5f};
            auto mesh = std::make_unique<MeshRenderer>();
            mesh->color = {0.95f, 0.55f, 0.20f, 1.0f};
            cube->addComponent(std::move(mesh));
        }

        if (!liveSimulation_) {
            scene_->camera().reset();
        }

        scene_->setLoadedScenePath("");
    }

    sceneDirty_ = false;
    updateWindowTitle();
}

bool Application::loadScene(const std::string& path, std::string& error)
{
    // Validate into a throwaway scene first so a bad/missing file leaves the
    // live scene untouched. Only on success do we rebuild and adopt it.
    Scene probe;
    probe.createCamera({0.0f, 3.0f, 5.0f});
    std::string warning;
    if (!SceneSerializer::load(probe, path, error, &warning)) {
        return false;
    }

    recreateScene();
    if (!SceneSerializer::load(*scene_, path, error, &warning)) {
        return false;
    }

    if (!liveSimulation_) {
        scene_->camera().reset();
    }
    if (renderer_) {
        scene_->camera().setAspect(renderer_->aspectRatio());
    }

    scene_->setLoadedScenePath(path);
    sceneDirty_ = false;
    Settings::addRecentScene(path);
    if (liveSimulation_) {
        Settings::saveLastScene(path);
    }
    updateWindowTitle();
    return true;
}

void Application::saveScene()
{
    if (!scene_) {
        return;
    }
    if (scene_->loadedScenePath().empty()) {
        // No path yet: ask the editor to open its Save As modal next frame.
        saveAsRequested_ = true;
        return;
    }

    std::string error;
    if (!SceneSerializer::save(*scene_, scene_->loadedScenePath(), error)) {
        std::fprintf(stderr, "Failed to save scene: %s\n", error.c_str());
        return;
    }

    sceneDirty_ = false;
    if (liveSimulation_) {
        Settings::saveLastScene(scene_->loadedScenePath());
    }
    updateWindowTitle();
}

bool Application::saveSceneAs(const std::string& filename, std::string& error)
{
    if (!scene_) {
        error = "No scene to save";
        return false;
    }

    std::string path = SceneIO::resolveScenePath(filename, error);
    if (path.empty()) {
        return false;
    }

    if (!SceneSerializer::save(*scene_, path, error)) {
        return false;
    }

    scene_->setLoadedScenePath(path);
    sceneDirty_ = false;
    Settings::addRecentScene(path);
    if (liveSimulation_) {
        Settings::saveLastScene(path);
    }
    updateWindowTitle();
    return true;
}

bool Application::consumeSaveAsRequest()
{
    if (!saveAsRequested_) {
        return false;
    }
    saveAsRequested_ = false;
    return true;
}

void Application::markSceneDirty()
{
    if (!sceneDirty_) {
        sceneDirty_ = true;
        updateWindowTitle();
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
        editor_->setLightSettings(&scene_->environment().light);
        editor_->setSkySettings(&scene_->environment().sky);
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

    if (input.forward || input.backward || input.left || input.right ||
        input.up || input.down || input.mouseDeltaX != 0.0f ||
        input.mouseDeltaY != 0.0f || input.scrollDelta != 0.0f) {
        markSceneDirty();
    }

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
    Vec3 dir = {scene_->environment().light.direction.x, scene_->environment().light.direction.y, scene_->environment().light.direction.z};
    ctx.setLight(dir, scene_->environment().light.ambient, scene_->environment().light.diffuse);
    ctx.setSky(scene_->environment().sky);
    scene_->buildRenderCommands(ctx, editor_->selected());
    lastRenderCommandCount_ = ctx.commands().size();

    renderer_->draw(ctx, clearColor_, [this](void* commandBuffer, void* renderEncoder, void* renderPassDescriptor) {
        editor_->render(commandBuffer, renderEncoder, renderPassDescriptor);
    });
}

void Application::onKeyEvent(int keyCode, bool down, bool shortcutModifier)
{
    if (ImGui::GetIO().WantCaptureKeyboard) {
        return;
    }

    if (shortcutModifier && down) {
        switch (keyCode) {
            case 's': case 'S': saveScene(); return;
            case 'n': case 'N': newScene(); return;
            default: break;
        }
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
    markSceneDirty();
}

void Application::onScroll(float delta)
{
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    scrollDelta_ += delta;
    markSceneDirty();
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
    markSceneDirty();
}
