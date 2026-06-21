#include "capture/capture.h"
#include "debug/bundle.h"
#include "debug/debug_state.h"
#include "agent/command.h"
#include "scene/camera.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
#include "scene/rotate_component.h"
#include "scene/scene.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using scene::MeshRenderer;

void runTestDebug()
{
    // DebugState formatting.
    {
        Scene scene;
        Camera* camera = scene.createCamera({0.0f, 1.0f, 2.0f});
        camera->setActive(false);
        GameObject* cube = scene.createObject("Cube");

        std::string text = DebugState::build(42, 60.0f, 16.66f, 2, scene, cube);
        assert(text.find("Workbench State") != std::string::npos);
        assert(text.find("[1] Camera") != std::string::npos);
        assert(text.find("[2] Cube") != std::string::npos);
        assert(text.find("selected: Cube [2]") != std::string::npos);
        assert(text.find("renderCommands: 2") != std::string::npos);

        const char* testPath = "build/test_state.txt";
        DebugState::writeToFile(text, testPath);
        assert(std::filesystem::exists(testPath));

        std::ifstream in(testPath);
        std::string content((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
        assert(content == text);
    }

    // DebugState lists components.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* cube = scene.createObject("Cube");
        cube->addComponent(std::make_unique<MeshRenderer>());

        std::string text = DebugState::build(1, 60.0f, 16.66f, 2, scene, nullptr);
        assert(text.find("components: MeshRenderer") != std::string::npos);
    }

    // DebugState includes loaded scene filename.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        scene.createObject("Cube");
        scene.setLoadedScenePath("test.scene");

        std::string text = DebugState::build(1, 60.0f, 16.66f, 2, scene, nullptr);
        assert(text.find("SceneFile: test.scene") != std::string::npos);
    }

    // DebugState reports last script path.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        std::string text = DebugState::build(1, 60.0f, 16.66f, 0, scene, nullptr, "repro.wbs");
        assert(text.find("Last Script: repro.wbs") != std::string::npos);
    }

    // Capture path generation.
    {
        std::filesystem::create_directories("captures");
        std::string named = Capture::makeCapturePath("test_scene.png");
        assert(named.find("captures/test_scene.png") != std::string::npos);

        std::string defaulted = Capture::makeCapturePath();
        assert(defaulted.find("captures/capture_") != std::string::npos);
        assert(defaulted.find(".png") != std::string::npos);
    }

    // Capture filename validation.
    {
        assert(Capture::isValidFilename("test.png"));
        assert(!Capture::isValidFilename(""));
        assert(!Capture::isValidFilename("foo/bar.png"));
        assert(!Capture::isValidFilename("..\\x.png"));
        assert(!Capture::isValidFilename("../secret.png"));
    }

    // PNG writing.
    {
        std::vector<uint8_t> pixels = {
            255, 0, 0, 255,   0, 255, 0, 255,
            0, 0, 255, 255,   255, 255, 255, 255
        };
        std::string path = Capture::makeCapturePath("write_test.png");
        assert(Capture::writePNG(path, pixels.data(), 2, 2));
        assert(std::filesystem::exists(path));
        std::filesystem::remove(path);
    }

    // render.capture command parsing and lastCapturePath tracking require a renderer.
    // Without one the command reports an error but the path is validated.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        AgentCommandResult result = executeCommand("render.capture bad/name.png", ctx);
        assert(!result.success);
        assert(result.output.find("Invalid") != std::string::npos);
        assert(ctx.lastCapturePath.empty());
    }

    // DebugState reports last capture path.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        std::string text = DebugState::build(1, 60.0f, 16.66f, 0, scene, nullptr, {}, "captures/shot.png");
        assert(text.find("Last Capture: captures/shot.png") != std::string::npos);
    }

    // Bundle path generation and validation.
    {
        std::string path = Bundle::makeBundlePath("cube_overlap");
        assert(path == "bundles/cube_overlap/");

        assert(Bundle::isValidBundleName("cube_overlap"));
        assert(!Bundle::isValidBundleName(""));
        assert(!Bundle::isValidBundleName("foo/bar"));
        assert(!Bundle::isValidBundleName(".."));
        assert(!Bundle::isValidBundleName("x/../y"));
    }

    // createBundle writes state.txt synchronously.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        std::string bundlePath;
        assert(Bundle::createBundle("test_bundle", ctx, bundlePath));
        assert(std::filesystem::exists(bundlePath + "state.txt"));

        std::ifstream in(bundlePath + "state.txt");
        std::string content((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
        assert(content.find("Workbench State") != std::string::npos);
        assert(content.find("Camera") != std::string::npos);

        std::filesystem::remove_all(bundlePath);
    }

    // debug.bundle command parsing and validation.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        AgentCommandResult result = executeCommand("debug.bundle", ctx);
        assert(!result.success);
        assert(result.output.find("Usage") != std::string::npos);

        result = executeCommand("debug.bundle bad/name", ctx);
        assert(!result.success);
        assert(result.output.find("Invalid") != std::string::npos);

        result = executeCommand("debug.bundle test_cmd", ctx);
        assert(!result.success);
        assert(result.output.find("Renderer not available") != std::string::npos);
    }

    // debug.bundle appears in command discovery.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        AgentCommandResult result = executeCommand("agent.commands", ctx);
        assert(result.success);
        assert(result.output.find("debug.bundle") != std::string::npos);

        result = executeCommand("agent.help debug.bundle", ctx);
        assert(result.success);
        assert(result.output.find("debug.bundle <name>") != std::string::npos);
    }

    // DebugState reports last bundle path.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        std::string text = DebugState::build(1, 60.0f, 16.66f, 0, scene, nullptr, {}, {}, "bundles/repro/");
        assert(text.find("Last Bundle: bundles/repro/") != std::string::npos);
    }

    // DebugState reports last assertion failure.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        std::string failure = "Expected object count: 1\nActual object count: 0";
        std::string text = DebugState::build(1, 60.0f, 16.66f, 0, scene, nullptr, {}, {}, {}, &failure);
        assert(text.find("Last Assertion Failure: Expected object count: 1") != std::string::npos);
    }

    // DebugState lists RotateComponent with angular velocity.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Spinner");
        auto rot = std::make_unique<scene::RotateComponent>();
        rot->angularVelocityEuler = {0.0f, 90.0f, 0.0f};
        obj->addComponent(std::move(rot));

        std::string text = DebugState::build(1, 60.0f, 16.66f, 0, scene, nullptr);
        assert(text.find("components: RotateComponent (angularVelocity: 0.00, 90.00, 0.00)") != std::string::npos);
    }

    // DebugState reports mesh shape for MeshRenderer.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("SphereObj");
        auto mesh = std::make_unique<MeshRenderer>();
        mesh->shape = scene::MeshShape::Sphere;
        obj->addComponent(std::move(mesh));

        std::string text = DebugState::build(1, 60.0f, 16.66f, 2, scene, nullptr);
        assert(text.find("components: MeshRenderer (mesh: sphere)") != std::string::npos);
    }

    // DebugState: Hierarchy section, parent: and worldPosition: lines.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* parent = scene.createObject("Parent");
        GameObject* child = scene.createObject("Child");
        child->transform().position = {1.0f, 0.0f, 0.0f};
        scene.setParent(child, parent);

        std::string text = DebugState::build(1, 60.0f, 16.66f, 0, scene, nullptr);
        assert(text.find("Hierarchy:") != std::string::npos);
        // Tree indentation: child is deeper than parent.
        assert(text.find("[2] Parent\n    [3] Child") != std::string::npos);
        assert(text.find("parent: Parent [2]") != std::string::npos);
        assert(text.find("parent: none") != std::string::npos);
        assert(text.find("worldPosition:") != std::string::npos);
    }
}
