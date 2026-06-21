#include "core/application.h"
#include "agent/command.h"
#include "agent/test_suite.h"
#include "core/math.h"
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

    lightSettings_.direction = {-0.5f, -1.0f, -0.75f};
    lightSettings_.ambient = 0.15f;
    lightSettings_.diffuse = 1.0f;

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

    // Simulation advances per frame only when no automation is requested.
    // In automation mode (--run-script/--bundle/--run-tests) the simulation
    // advances exclusively via `sim.step` for determinism.
    liveSimulation_ = cliOptions_.runScript.empty()
                   && cliOptions_.bundleName.empty()
                   && !cliOptions_.runTests;
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
