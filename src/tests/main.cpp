#include "agent/command.h"
#include "agent/script_runner.h"
#include "agent/test_suite.h"
#include "capture/capture.h"
#include "core/cli_options.h"
#include "core/math.h"
#include "core/object_id.h"
#include "debug/bundle.h"
#include "debug/debug_state.h"
#include "renderer/mesh_geometry.h"
#include "renderer/render_context.h"
#include "scene/camera.h"
#include "scene/component.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
#include "scene/rotate_component.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "scene/transform.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using scene::MeshRenderer;

int main()
{
    // ObjectId uniqueness via Scene allocation.
    {
        Scene scene;
        GameObject* a = scene.createObject("A");
        GameObject* b = scene.createObject("B");
        GameObject* c = scene.createObject("C");

        assert(a->id().isValid());
        assert(b->id().isValid());
        assert(c->id().isValid());
        assert(a->id() != b->id());
        assert(b->id() != c->id());
        assert(a->id() != c->id());
    }

    // Transform defaults.
    {
        Transform t;
        assert(t.position.x == 0.0f);
        assert(t.position.y == 0.0f);
        assert(t.position.z == 0.0f);
        assert(t.scale.x == 1.0f);
        assert(t.scale.y == 1.0f);
        assert(t.scale.z == 1.0f);

        Mat4 m = t.localMatrix();
        assert(m.columns[0][0] == 1.0f);
        assert(m.columns[1][1] == 1.0f);
        assert(m.columns[2][2] == 1.0f);
        assert(m.columns[3][3] == 1.0f);
    }

    // Scene object creation.
    {
        Scene scene;
        GameObject* cube = scene.createObject("Cube");
        assert(cube != nullptr);
        assert(cube->name() == "Cube");
        assert(cube->active());
        assert(scene.objects().size() == 1);

        Camera* camera = scene.createCamera({0.0f, 1.0f, 2.0f});
        assert(camera != nullptr);
        assert(camera->name() == "Camera");
        assert(scene.objects().size() == 2);
    }

    // RenderContext command accumulation.
    {
        RenderContext ctx;
        ctx.setCamera(identity(), identity());
        ctx.drawShape(ShapeType::Cube, identity(), {1.0f, 0.0f, 0.0f, 1.0f});
        ctx.drawShape(ShapeType::Cube, identity(), {0.0f, 1.0f, 0.0f, 1.0f});
        ctx.drawGround({0.0f, 0.0f, 0.0f});

        assert(ctx.commands().size() == 3);
    }

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

    // Agent command interface.
    {
        Scene scene;
        Camera* camera = scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* cube = scene.createObject("Cube");
        (void)camera;

        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        // scene.list
        AgentCommandResult result = executeCommand("scene.list", ctx);
        assert(result.success);
        assert(result.output.find("[1] Camera") != std::string::npos);
        assert(result.output.find("[2] Cube") != std::string::npos);

        // scene.select
        result = executeCommand("scene.select 2", ctx);
        assert(result.success);
        assert(selected == cube);
        assert(result.output.find("Selected: Cube [2]") != std::string::npos);

        // scene.get_selected
        result = executeCommand("scene.get_selected", ctx);
        assert(result.success);
        assert(result.output.find("Cube [2]") != std::string::npos);

        // transform.set_position
        result = executeCommand("transform.set_position 2 1.5 2.5 3.5", ctx);
        assert(result.success);
        assert(cube->transform().position.x == 1.5f);
        assert(cube->transform().position.y == 2.5f);
        assert(cube->transform().position.z == 3.5f);

        // transform.get
        result = executeCommand("transform.get 2", ctx);
        assert(result.success);
        assert(result.output.find("Position: 1.50, 2.50, 3.50") != std::string::npos);

        // debug.dump
        result = executeCommand("debug.dump", ctx);
        assert(result.success);
        assert(result.output.find("Workbench State") != std::string::npos);
        assert(result.output.find("selected: Cube [2]") != std::string::npos);

        // Unknown command.
        result = executeCommand("not.a.command", ctx);
        assert(!result.success);
    }

    // Command discovery.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        scene.createObject("Cube");

        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        AgentCommandResult result = executeCommand("agent.commands", ctx);
        assert(result.success);
        assert(result.output.find("agent.commands") != std::string::npos);
        assert(result.output.find("agent.help") != std::string::npos);
        assert(result.output.find("scene.list") != std::string::npos);
        assert(result.output.find("transform.set_position") != std::string::npos);

        result = executeCommand("agent.help", ctx);
        assert(result.success);
        assert(result.output.find("Workbench Agent Interface") != std::string::npos);
        assert(result.output.find("agent.commands") != std::string::npos);

        result = executeCommand("agent.help scene.select", ctx);
        assert(result.success);
        assert(result.output.find("scene.select <id>") != std::string::npos);
        assert(result.output.find("Selects a GameObject by ObjectId") != std::string::npos);
        assert(result.output.find("scene.select 2") != std::string::npos);

        result = executeCommand("agent.help not_a_command", ctx);
        assert(!result.success);
    }

    // Scene authoring operations.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});

        GameObject* a = scene.createObject("A");
        a->transform().position = {1.0f, 2.0f, 3.0f};
        a->transform().scale = {2.0f, 2.0f, 2.0f};
        auto meshA = std::make_unique<MeshRenderer>();
        meshA->color = {0.1f, 0.2f, 0.3f, 0.4f};
        a->addComponent(std::move(meshA));

        GameObject* copy = scene.duplicateObject(a);
        assert(copy != nullptr);
        assert(copy->name() == "A Copy");
        assert(copy->transform().position.x == 1.0f);
        assert(copy->transform().scale.x == 2.0f);
        MeshRenderer* copyMesh = copy->getComponent<MeshRenderer>();
        assert(copyMesh != nullptr);
        assert(copyMesh->color.x == 0.1f);
        assert(scene.objects().size() == 3);

        assert(scene.deleteObject(a));
        assert(scene.objects().size() == 2);
        assert(scene.findObjectById(a->id().value) == nullptr);
    }

    // Scene save/load.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});

        GameObject* a = scene.createObject("A");
        a->transform().position = {1.5f, 2.5f, 3.5f};
        a->transform().rotation = {0.1f, 0.2f, 0.3f};
        a->transform().scale = {2.0f, 1.0f, 0.5f};
        auto meshA = std::make_unique<MeshRenderer>();
        meshA->color = {0.1f, 0.2f, 0.3f, 0.4f};
        a->addComponent(std::move(meshA));

        const char* path = "build/tests/test_scene.scene";
        std::string error;
        assert(SceneSerializer::save(scene, path, error));
        assert(std::filesystem::exists(path));

        Scene loaded;
        loaded.createCamera({0.0f, 0.0f, 0.0f});
        loaded.createObject("WillBeCleared");
        assert(SceneSerializer::load(loaded, path, error));

        assert(loaded.objects().size() == 2);
        GameObject* loadedA = nullptr;
        for (const auto& obj : loaded.objects()) {
            if (!loaded.isCamera(obj.get())) {
                loadedA = obj.get();
            }
        }
        assert(loadedA != nullptr);
        assert(loadedA->name() == "A");
        assert(loadedA->transform().position.x == 1.5f);
        assert(loadedA->transform().position.y == 2.5f);
        assert(loadedA->transform().position.z == 3.5f);
        assert(loadedA->transform().rotation.x == 0.1f);
        assert(loadedA->transform().rotation.y == 0.2f);
        assert(loadedA->transform().rotation.z == 0.3f);
        assert(loadedA->transform().scale.x == 2.0f);
        assert(loadedA->transform().scale.y == 1.0f);
        assert(loadedA->transform().scale.z == 0.5f);
        MeshRenderer* loadedMesh = loadedA->getComponent<MeshRenderer>();
        assert(loadedMesh != nullptr);
        assert(loadedMesh->color.x == 0.1f);
        assert(loadedMesh->color.y == 0.2f);
        assert(loadedMesh->color.z == 0.3f);
        assert(loadedMesh->color.w == 0.4f);

        std::filesystem::remove(path);
    }

    // Scene authoring agent commands.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        AgentCommandResult result = executeCommand("scene.create_cube TestA", ctx);
        assert(result.success);
        assert(selected != nullptr);
        assert(selected->name() == "TestA");
        selected->transform().position = {1.0f, 0.0f, 0.0f};

        uint64_t id = selected->id().value;
        result = executeCommand("scene.duplicate " + std::to_string(id), ctx);
        assert(result.success);
        assert(selected != nullptr);
        assert(selected->name() == "TestA Copy");

        uint64_t copyId = selected->id().value;
        result = executeCommand("scene.delete " + std::to_string(copyId), ctx);
        assert(result.success);
        assert(selected == nullptr);

        result = executeCommand("scene.save test_cmd.scene", ctx);
        assert(result.success);
        assert(scene.loadedScenePath() == "test_cmd.scene");

        selected = scene.findObjectById(id);
        assert(selected != nullptr);
        result = executeCommand("scene.load test_cmd.scene", ctx);
        assert(result.success);
        assert(selected == nullptr);
        assert(scene.objects().size() == 2);

        std::filesystem::remove("assets/scenes/test_cmd.scene");
    }

    // Component agent commands.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* obj = scene.createObject("Plain");
        (void)obj;

        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        AgentCommandResult result = executeCommand("component.list 2", ctx);
        assert(result.success);
        assert(result.output.find("(none)") != std::string::npos);

        result = executeCommand("component.add_mesh_renderer 2", ctx);
        assert(result.success);
        assert(obj->hasComponent<MeshRenderer>());

        result = executeCommand("component.list 2", ctx);
        assert(result.success);
        assert(result.output.find("MeshRenderer") != std::string::npos);

        result = executeCommand("component.add_mesh_renderer 2", ctx);
        assert(!result.success);
    }

    // Load warns about objects with no MeshRenderer.
    {
        const char* path = "build/tests/warning_scene.scene";
        std::ofstream out(path);
        out << R"({"objects": [{"name": "Invisible", "position": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1], "components": []}]})";
        out.close();

        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        std::string error;
        std::string warning;
        assert(SceneSerializer::load(scene, path, error, &warning));
        assert(!warning.empty());
        assert(warning.find("Invisible") != std::string::npos);
        assert(warning.find("no MeshRenderer") != std::string::npos);

        std::filesystem::remove(path);
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

    // Script runner: success, blank lines, comments, scene mutation.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        std::filesystem::create_directories("build/tests");
        const char* path = "build/tests/test_script.wbs";
        std::ofstream out(path);
        out << "# Create a cube\n\n";
        out << "scene.create_cube ScriptCube\n";
        out << "transform.set_position 2 1.0 2.0 3.0\n";
        out.close();

        ScriptResult result = runScript(ctx, path, "test_script.wbs");
        assert(result.success);
        assert(result.executed == 2);
        assert(scene.objects().size() == 2);
        GameObject* cube = scene.findObjectById(2);
        assert(cube != nullptr);
        assert(cube->name() == "ScriptCube");
        assert(cube->transform().position.x == 1.0f);
        assert(cube->transform().position.y == 2.0f);
        assert(cube->transform().position.z == 3.0f);
        assert(ctx.lastScriptPath == "test_script.wbs");

        std::filesystem::remove(path);
    }

    // Script runner: stops on first failure.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        const char* path = "build/tests/failing_script.wbs";
        std::ofstream out(path);
        out << "scene.create_cube A\n";
        out << "scene.delete 99\n";
        out.close();

        ScriptResult result = runScript(ctx, path, "failing_script.wbs");
        assert(!result.success);
        assert(result.executed == 1);
        assert(result.error.find("99") != std::string::npos);

        std::filesystem::remove(path);
    }

    // script.run agent command.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        std::filesystem::create_directories("assets/scripts");
        const char* path = "assets/scripts/agent_script.wbs";
        std::ofstream out(path);
        out << "scene.create_cube AgentCube\n";
        out.close();

        AgentCommandResult result = executeCommand("script.run agent_script.wbs", ctx);
        assert(result.success);
        assert(result.output.find("Executed 1 command") != std::string::npos);
        assert(scene.findObjectById(2) != nullptr);
        assert(ctx.lastScriptPath == "agent_script.wbs");

        std::filesystem::remove(path);
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

    // Assertion commands.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* cube = scene.createObject("Cube");
        cube->addComponent(std::make_unique<MeshRenderer>());

        GameObject* selected = cube;
        std::string lastFailure;
        AgentCommandContext ctx{scene, selected, 0, 0.0f, 0.0f, 0, {}, nullptr, {}, {}, &lastFailure};

        // assert.object_count success (camera + cube = 2).
        AgentCommandResult result = executeCommand("assert.object_count 2", ctx);
        assert(result.success);
        assert(result.output == "OK");

        // assert.object_count failure.
        result = executeCommand("assert.object_count 5", ctx);
        assert(!result.success);
        assert(result.output.find("Expected object count: 5") != std::string::npos);
        assert(result.output.find("Actual object count: 2") != std::string::npos);
        assert(lastFailure == result.output);

        // assert.object_exists success.
        result = executeCommand("assert.object_exists Cube", ctx);
        assert(result.success);
        assert(result.output == "OK");

        // assert.object_exists failure.
        lastFailure.clear();
        result = executeCommand("assert.object_exists Missing", ctx);
        assert(!result.success);
        assert(result.output.find("Object not found: Missing") != std::string::npos);
        assert(lastFailure == result.output);

        // assert.selected success.
        result = executeCommand("assert.selected Cube", ctx);
        assert(result.success);
        assert(result.output == "OK");

        // assert.selected failure.
        lastFailure.clear();
        result = executeCommand("assert.selected Wrong", ctx);
        assert(!result.success);
        assert(result.output.find("Expected selection: Wrong") != std::string::npos);
        assert(result.output.find("Actual selection: Cube") != std::string::npos);

        // assert.has_component success.
        result = executeCommand("assert.has_component Cube MeshRenderer", ctx);
        assert(result.success);
        assert(result.output == "OK");

        // assert.has_component failure.
        lastFailure.clear();
        GameObject* plain = scene.createObject("Plain");
        (void)plain;
        result = executeCommand("assert.has_component Plain MeshRenderer", ctx);
        assert(!result.success);
        assert(result.output.find("Plain does not have component MeshRenderer") != std::string::npos);
    }

    // Script runner: assertion failure stops execution.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        std::filesystem::create_directories("build/tests");
        const char* path = "build/tests/assert_fail.wbs";
        std::ofstream out(path);
        out << "assert.object_exists Cube\n";
        out << "scene.create_cube Cube\n";
        out.close();

        ScriptResult result = runScript(ctx, path, "assert_fail.wbs");
        assert(!result.success);
        assert(result.error.find("Object not found: Cube") != std::string::npos);
        assert(result.executed == 0);

        std::filesystem::remove(path);
    }

    // DebugState reports last assertion failure.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        std::string failure = "Expected object count: 1\nActual object count: 0";
        std::string text = DebugState::build(1, 60.0f, 16.66f, 0, scene, nullptr, {}, {}, {}, &failure);
        assert(text.find("Last Assertion Failure: Expected object count: 1") != std::string::npos);
    }

    // Test suite discovery.
    {
        std::filesystem::create_directories("build/tests/discover");
        std::ofstream("build/tests/discover/beta.wbs").close();
        std::ofstream("build/tests/discover/alpha.wbs").close();
        std::ofstream("build/tests/discover/ignore.txt").close();

        auto tests = TestSuite::discoverTests("build/tests/discover");
        assert(tests.size() == 2);
        assert(tests[0].filename() == "alpha.wbs");
        assert(tests[1].filename() == "beta.wbs");

        std::filesystem::remove_all("build/tests/discover");
    }

    // Test suite pass/fail counting.
    {
        std::filesystem::create_directories("build/tests/suite");

        {
            std::ofstream pass("build/tests/suite/pass.wbs");
            pass << "scene.create_cube TestCube\n";
            pass << "assert.object_exists TestCube\n";
        }

        {
            std::ofstream fail("build/tests/suite/fail.wbs");
            fail << "assert.object_count 99\n";
        }

        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        auto passResult = TestSuite::runTestFile(scene, ctx, "build/tests/suite/pass.wbs");
        assert(passResult.passed);
        assert(passResult.name == "pass");

        auto failResult = TestSuite::runTestFile(scene, ctx, "build/tests/suite/fail.wbs");
        assert(!failResult.passed);
        assert(failResult.name == "fail");
        assert(failResult.failure.find("Expected object count: 99") != std::string::npos);

        TestSuite::Summary summary;
        summary.results.push_back(std::move(passResult));
        summary.results.push_back(std::move(failResult));
        assert(summary.passedCount() == 1);
        assert(summary.failedCount() == 1);
        assert(!summary.allPassed());

        std::filesystem::remove_all("build/tests/suite");
    }

    // Test suite failure bundle writes state.txt.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        TestSuite::Result result = TestSuite::runTestFile(scene, ctx, "assets/scripts/assertion_fail.wbs");
        assert(!result.passed);

        std::string bundlePath;
        assert(TestSuite::createFailureBundle("test_state", ctx, bundlePath));
        assert(std::filesystem::exists(bundlePath + "state.txt"));

        std::ifstream in(bundlePath + "state.txt");
        std::string content((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
        assert(content.find("Workbench State") != std::string::npos);

        std::filesystem::remove_all(bundlePath);
    }

    // CLI option parsing.
    {
        const char* args[] = {"sandbox", "--run-script", "create_test_scene.wbs", "--bundle", "cli_smoke", "--exit", "--frames", "5"};
        CliOptions opts = parseCliOptions(8, args);
        assert(opts.runScript == "create_test_scene.wbs");
        assert(opts.bundleName == "cli_smoke");
        assert(opts.autoExit);
        assert(opts.extraFrames == 5);
        assert(!opts.runTests);
    }

    // CLI --run-tests flag.
    {
        const char* args[] = {"sandbox", "--run-tests", "--exit"};
        CliOptions opts = parseCliOptions(3, args);
        assert(opts.runTests);
        assert(opts.autoExit);
    }

    // CLI defaults.
    {
        const char* args[] = {"sandbox"};
        CliOptions opts = parseCliOptions(1, args);
        assert(opts.runScript.empty());
        assert(opts.bundleName.empty());
        assert(!opts.autoExit);
        assert(opts.extraFrames == 3);
    }

    // CLI missing arguments.
    {
        const char* args[] = {"sandbox", "--run-script", "--bundle", "name"};
        CliOptions opts = parseCliOptions(4, args);
        assert(opts.runScript.empty());
        assert(opts.bundleName == "name");
    }

    // Component lifecycle: onStart once, onUpdate each step.
    {
        // A tiny test component that records lifecycle transitions.
        struct CountingComponent : scene::Component {
            const char* typeName() const override { return "CountingComponent"; }
            std::unique_ptr<Component> clone() const override { return std::make_unique<CountingComponent>(*this); }
            void onStart() override { ++starts; }
            void onUpdate(float) override { ++updates; }
            int starts = 0;
            int updates = 0;
        };

        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Obj");
        obj->addComponent(std::make_unique<CountingComponent>());

        CountingComponent* cc = obj->getComponent<CountingComponent>();
        assert(cc->starts == 0);
        assert(cc->updates == 0);

        scene.update(0.1f);
        assert(cc->starts == 1);
        assert(cc->updates == 1);

        scene.update(0.1f);
        assert(cc->starts == 1);
        assert(cc->updates == 2);
    }

    // Component lifecycle: inactive objects are skipped.
    {
        struct CountingComponent : scene::Component {
            const char* typeName() const override { return "CountingComponent"; }
            std::unique_ptr<Component> clone() const override { return std::make_unique<CountingComponent>(*this); }
            void onUpdate(float) override { ++updates; }
            int updates = 0;
        };

        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Obj");
        obj->setActive(false);
        obj->addComponent(std::make_unique<CountingComponent>());

        CountingComponent* cc = obj->getComponent<CountingComponent>();
        scene.update(0.1f);
        scene.update(0.1f);
        assert(cc->updates == 0);
    }

    // RotateComponent defaults and typeName.
    {
        scene::RotateComponent rot;
        assert(std::string(rot.typeName()) == "RotateComponent");
        assert(rot.angularVelocityEuler.x == 0.0f);
        assert(rot.angularVelocityEuler.y == 0.0f);
        assert(rot.angularVelocityEuler.z == 0.0f);
    }

    // RotateComponent::onUpdate advances rotation in radians from deg/s.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Spinner");
        auto rot = std::make_unique<scene::RotateComponent>();
        rot->angularVelocityEuler = {0.0f, 90.0f, 0.0f};
        obj->addComponent(std::move(rot));

        scene.update(1.0f);
        float expectedY = 90.0f * kDegToRad;
        assert(std::fabs(obj->transform().rotation.y - expectedY) < 1e-4f);
        assert(obj->transform().rotation.x == 0.0f);
        assert(obj->transform().rotation.z == 0.0f);

        scene.update(1.0f);
        assert(std::fabs(obj->transform().rotation.y - 2.0f * expectedY) < 1e-4f);
    }

    // RotateComponent::clone preserves angular velocity.
    {
        scene::RotateComponent rot;
        rot.angularVelocityEuler = {10.0f, 20.0f, 30.0f};
        auto copy = rot.clone();
        auto* rc = dynamic_cast<scene::RotateComponent*>(copy.get());
        assert(rc != nullptr);
        assert(rc->angularVelocityEuler.x == 10.0f);
        assert(rc->angularVelocityEuler.y == 20.0f);
        assert(rc->angularVelocityEuler.z == 30.0f);
    }

    // component.add_rotator and component.set_rotator commands.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Spinner");
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        AgentCommandResult result = executeCommand("component.add_rotator Spinner", ctx);
        assert(result.success);
        assert(obj->hasComponent<scene::RotateComponent>());

        // Adding twice fails.
        result = executeCommand("component.add_rotator Spinner", ctx);
        assert(!result.success);

        // Missing object.
        result = executeCommand("component.add_rotator Nope", ctx);
        assert(!result.success);

        // Set angular velocity.
        result = executeCommand("component.set_rotator Spinner 0 90 0", ctx);
        assert(result.success);
        scene::RotateComponent* rc = obj->getComponent<scene::RotateComponent>();
        assert(rc->angularVelocityEuler.y == 90.0f);

        // Set on missing object / missing component.
        result = executeCommand("component.set_rotator Nope 0 0 0", ctx);
        assert(!result.success);
        GameObject* plain = scene.createObject("Plain");
        (void)plain;
        result = executeCommand("component.set_rotator Plain 0 0 0", ctx);
        assert(!result.success);
    }

    // sim.step advances the simulation deterministically.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Spinner");
        auto rot = std::make_unique<scene::RotateComponent>();
        rot->angularVelocityEuler = {0.0f, 90.0f, 0.0f};
        obj->addComponent(std::move(rot));

        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        AgentCommandResult result = executeCommand("sim.step 1.0", ctx);
        assert(result.success);
        float expectedY = 90.0f * kDegToRad;
        assert(std::fabs(obj->transform().rotation.y - expectedY) < 1e-4f);
    }

    // assert.rotation success and failure.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Spinner");
        auto rot = std::make_unique<scene::RotateComponent>();
        rot->angularVelocityEuler = {10.0f, 20.0f, 30.0f};
        obj->addComponent(std::move(rot));

        GameObject* selected = nullptr;
        std::string lastFailure;
        AgentCommandContext ctx{scene, selected, 0, 0.0f, 0.0f, 0, {}, nullptr, {}, {}, &lastFailure};

        executeCommand("sim.step 1.0", ctx);

        AgentCommandResult result = executeCommand("assert.rotation Spinner 10 20 30", ctx);
        assert(result.success);
        assert(result.output == "OK");

        // Failure with tolerance.
        lastFailure.clear();
        result = executeCommand("assert.rotation Spinner 10 20 99 0.01", ctx);
        assert(!result.success);
        assert(result.output.find("Expected rotation: 10.0000, 20.0000, 99.0000") != std::string::npos);
        assert(lastFailure == result.output);

        // Loose tolerance passes.
        result = executeCommand("assert.rotation Spinner 10 20 35 100.0", ctx);
        assert(result.success);

        // Missing object is an assertion failure.
        lastFailure.clear();
        result = executeCommand("assert.rotation Nope 0 0 0", ctx);
        assert(!result.success);
        assert(result.output.find("Object not found: Nope") != std::string::npos);
    }

    // assert.position success and failure.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Cube");
        obj->transform().position = {1.0f, 2.0f, 3.0f};

        GameObject* selected = nullptr;
        std::string lastFailure;
        AgentCommandContext ctx{scene, selected, 0, 0.0f, 0.0f, 0, {}, nullptr, {}, {}, &lastFailure};

        AgentCommandResult result = executeCommand("assert.position Cube 1 2 3", ctx);
        assert(result.success);
        assert(result.output == "OK");

        // Failure within default tolerance.
        lastFailure.clear();
        result = executeCommand("assert.position Cube 1 2 3.5", ctx);
        assert(!result.success);
        assert(result.output.find("Expected position: 1.0000, 2.0000, 3.5000") != std::string::npos);
        assert(lastFailure == result.output);

        // Loose tolerance passes.
        result = executeCommand("assert.position Cube 1 2 3.4 1.0", ctx);
        assert(result.success);

        // Missing object is an assertion failure.
        lastFailure.clear();
        result = executeCommand("assert.position Nope 0 0 0", ctx);
        assert(!result.success);
        assert(result.output.find("Object not found: Nope") != std::string::npos);
    }

    // assert.scale success and failure.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Cube");
        obj->transform().scale = {2.0f, 4.0f, 6.0f};

        GameObject* selected = nullptr;
        std::string lastFailure;
        AgentCommandContext ctx{scene, selected, 0, 0.0f, 0.0f, 0, {}, nullptr, {}, {}, &lastFailure};

        AgentCommandResult result = executeCommand("assert.scale Cube 2 4 6", ctx);
        assert(result.success);
        assert(result.output == "OK");

        // Failure within default tolerance.
        lastFailure.clear();
        result = executeCommand("assert.scale Cube 2 4 6.5", ctx);
        assert(!result.success);
        assert(result.output.find("Expected scale: 2.0000, 4.0000, 6.5000") != std::string::npos);
        assert(lastFailure == result.output);

        // Loose tolerance passes.
        result = executeCommand("assert.scale Cube 2 4 6.4 1.0", ctx);
        assert(result.success);

        // Missing object is an assertion failure.
        lastFailure.clear();
        result = executeCommand("assert.scale Nope 1 1 1", ctx);
        assert(!result.success);
        assert(result.output.find("Object not found: Nope") != std::string::npos);
    }

    // assert.color success, failure, and missing MeshRenderer.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Cube");
        auto mesh = std::make_unique<MeshRenderer>();
        mesh->color = {0.95f, 0.55f, 0.20f, 1.0f};
        obj->addComponent(std::move(mesh));

        GameObject* selected = nullptr;
        std::string lastFailure;
        AgentCommandContext ctx{scene, selected, 0, 0.0f, 0.0f, 0, {}, nullptr, {}, {}, &lastFailure};

        AgentCommandResult result = executeCommand("assert.color Cube 0.95 0.55 0.20 1.0", ctx);
        assert(result.success);
        assert(result.output == "OK");

        // Failure within default tolerance.
        lastFailure.clear();
        result = executeCommand("assert.color Cube 0.95 0.55 0.20 0.5", ctx);
        assert(!result.success);
        assert(result.output.find("Expected color: 0.9500, 0.5500, 0.2000, 0.5000") != std::string::npos);
        assert(lastFailure == result.output);

        // Loose tolerance passes.
        result = executeCommand("assert.color Cube 0.95 0.55 0.20 0.6 0.5", ctx);
        assert(result.success);

        // Missing object is an assertion failure.
        lastFailure.clear();
        result = executeCommand("assert.color Nope 1 1 1 1", ctx);
        assert(!result.success);
        assert(result.output.find("Object not found: Nope") != std::string::npos);

        // Existing object without a MeshRenderer is an assertion failure.
        scene.createObject("Bare");
        lastFailure.clear();
        result = executeCommand("assert.color Bare 1 1 1 1", ctx);
        assert(!result.success);
        assert(result.output.find("Object 'Bare' has no MeshRenderer component.") != std::string::npos);
        assert(lastFailure == result.output);
    }

    // RotateComponent serialization round-trip.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Spinner");
        auto rot = std::make_unique<scene::RotateComponent>();
        rot->angularVelocityEuler = {0.0f, 90.0f, 0.0f};
        obj->addComponent(std::move(rot));

        const char* path = "build/tests/rotator_scene.scene";
        std::string error;
        assert(SceneSerializer::save(scene, path, error));

        Scene loaded;
        loaded.createCamera({0.0f, 0.0f, 0.0f});
        assert(SceneSerializer::load(loaded, path, error));

        GameObject* loadedObj = nullptr;
        for (const auto& o : loaded.objects()) {
            if (!loaded.isCamera(o.get())) loadedObj = o.get();
        }
        assert(loadedObj != nullptr);
        scene::RotateComponent* lr = loadedObj->getComponent<scene::RotateComponent>();
        assert(lr != nullptr);
        assert(lr->angularVelocityEuler.x == 0.0f);
        assert(lr->angularVelocityEuler.y == 90.0f);
        assert(lr->angularVelocityEuler.z == 0.0f);

        std::filesystem::remove(path);
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

    // New commands appear in command discovery.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        AgentCommandResult result = executeCommand("agent.commands", ctx);
        assert(result.success);
        assert(result.output.find("component.add_rotator") != std::string::npos);
        assert(result.output.find("component.set_rotator") != std::string::npos);
        assert(result.output.find("sim.step") != std::string::npos);
        assert(result.output.find("assert.rotation") != std::string::npos);
        assert(result.output.find("assert.position") != std::string::npos);
        assert(result.output.find("assert.scale") != std::string::npos);
        assert(result.output.find("assert.color") != std::string::npos);
    }

    // ===== Milestone 0.7: Primitive Mesh Library =====

    // Mesh geometry generation: counts, bounds, index validity, and normals.
    {
        MeshData cube = makeCube();
        assert(cube.vertices.size() == 24);
        assert(cube.indices.size() == 36);
        for (const auto& v : cube.vertices) {
            assert(v.position[0] >= -1.0f && v.position[0] <= 1.0f);
            assert(v.position[1] >= -1.0f && v.position[1] <= 1.0f);
            assert(v.position[2] >= -1.0f && v.position[2] <= 1.0f);

            float nx = v.normal[0];
            float ny = v.normal[1];
            float nz = v.normal[2];
            float nLen = std::sqrt(nx * nx + ny * ny + nz * nz);
            assert(std::fabs(nLen - 1.0f) < 1e-4f);

            // Cube face normals are axis-aligned.
            assert((std::fabs(nx) == 1.0f && ny == 0.0f && nz == 0.0f) ||
                   (nx == 0.0f && std::fabs(ny) == 1.0f && nz == 0.0f) ||
                   (nx == 0.0f && ny == 0.0f && std::fabs(nz) == 1.0f));
        }
        for (uint16_t idx : cube.indices) {
            assert(idx < cube.vertices.size());
        }
        // Each face's 4 vertices share the same normal.
        for (int face = 0; face < 6; ++face) {
            size_t base = face * 4;
            for (size_t i = 1; i < 4; ++i) {
                assert(cube.vertices[base + i].normal[0] == cube.vertices[base].normal[0]);
                assert(cube.vertices[base + i].normal[1] == cube.vertices[base].normal[1]);
                assert(cube.vertices[base + i].normal[2] == cube.vertices[base].normal[2]);
            }
        }

        MeshData sphere = makeSphere();
        constexpr int kSphereSlices = 16;
        constexpr int kSphereStacks = 12;
        assert(sphere.vertices.size() == static_cast<size_t>((kSphereStacks + 1) * (kSphereSlices + 1)));
        assert(sphere.indices.size() == static_cast<size_t>(kSphereStacks * kSphereSlices * 6));
        for (const auto& v : sphere.vertices) {
            float x = v.position[0];
            float y = v.position[1];
            float z = v.position[2];
            assert(x >= -1.0f && x <= 1.0f);
            assert(y >= -1.0f && y <= 1.0f);
            assert(z >= -1.0f && z <= 1.0f);
            float len = std::sqrt(x * x + y * y + z * z);
            assert(std::fabs(len - 1.0f) < 1e-4f);

            float nx = v.normal[0];
            float ny = v.normal[1];
            float nz = v.normal[2];
            float nLen = std::sqrt(nx * nx + ny * ny + nz * nz);
            assert(std::fabs(nLen - 1.0f) < 1e-4f);
            assert(std::fabs(nx - x) < 1e-4f);
            assert(std::fabs(ny - y) < 1e-4f);
            assert(std::fabs(nz - z) < 1e-4f);
        }
        for (uint16_t idx : sphere.indices) {
            assert(idx < sphere.vertices.size());
        }

        MeshData plane = makePlane();
        assert(plane.vertices.size() == 4);
        assert(plane.indices.size() == 6);
        for (const auto& v : plane.vertices) {
            assert(v.position[0] >= -1.0f && v.position[0] <= 1.0f);
            assert(v.position[2] >= -1.0f && v.position[2] <= 1.0f);
            assert(v.position[1] == 0.0f);

            assert(v.normal[0] == 0.0f);
            assert(v.normal[1] == 1.0f);
            assert(v.normal[2] == 0.0f);
        }
        for (uint16_t idx : plane.indices) {
            assert(idx < plane.vertices.size());
        }
    }

    // Vertex layout for lighting.
    {
        assert(sizeof(Vertex) == 24);
        assert(alignof(Vertex) == 4);
    }

    // Normal matrix: rotation + uniform scale.
    {
        Mat4 m = multiply(rotationY(kPi / 2.0f), scale(2.0f, 2.0f, 2.0f));
        Mat3 nm = normalMatrix(m);

        Vec3 localN = {1.0f, 0.0f, 0.0f};
        simd::float3 worldN = nm * simd::float3{localN.x, localN.y, localN.z};
        worldN = simd::normalize(worldN);

        // A +X normal under a 90-degree Y rotation points +Z.
        assert(std::fabs(worldN.x) < 1e-4f);
        assert(std::fabs(worldN.y) < 1e-4f);
        assert(std::fabs(worldN.z - 1.0f) < 1e-4f);
    }

    // Normal matrix: non-uniform scale.
    {
        Mat4 m = scale(2.0f, 1.0f, 0.5f);
        Mat3 nm = normalMatrix(m);

        // A face normal in +X direction. Under non-uniform scale the normal
        // must remain perpendicular to the scaled tangent in world space.
        Vec3 localN = {1.0f, 0.0f, 0.0f};
        simd::float3 worldN = nm * simd::float3{localN.x, localN.y, localN.z};
        worldN = simd::normalize(worldN);

        // Tangent along +Z on the +X face; in world space it is stretched by 0.5x.
        Vec3 localT = {0.0f, 0.0f, 1.0f};
        simd::float3 worldT = m.columns[2].xyz;

        float perpendicular = simd::dot(worldN, worldT);
        assert(std::fabs(perpendicular) < 1e-4f);
        assert(std::fabs(worldN.x - 1.0f) < 1e-4f);
        assert(std::fabs(worldN.y) < 1e-4f);
        assert(std::fabs(worldN.z) < 1e-4f);
    }

    // Mesh shape string mapping.
    {
        scene::MeshShape shape = scene::MeshShape::Cube;
        assert(std::string(scene::meshShapeToString(scene::MeshShape::Cube)) == "cube");
        assert(std::string(scene::meshShapeToString(scene::MeshShape::Sphere)) == "sphere");
        assert(std::string(scene::meshShapeToString(scene::MeshShape::Plane)) == "plane");

        assert(scene::meshShapeFromString("cube", shape));
        assert(shape == scene::MeshShape::Cube);
        assert(scene::meshShapeFromString("sphere", shape));
        assert(shape == scene::MeshShape::Sphere);
        assert(scene::meshShapeFromString("plane", shape));
        assert(shape == scene::MeshShape::Plane);

        shape = scene::MeshShape::Cube;
        assert(!scene::meshShapeFromString("teapot", shape));
        assert(shape == scene::MeshShape::Cube); // unchanged on failure
    }

    // MeshRenderer defaults and clone preserve shape.
    {
        MeshRenderer mr;
        assert(mr.shape == scene::MeshShape::Cube);

        mr.shape = scene::MeshShape::Sphere;
        auto copy = mr.clone();
        auto* copyMesh = dynamic_cast<MeshRenderer*>(copy.get());
        assert(copyMesh != nullptr);
        assert(copyMesh->shape == scene::MeshShape::Sphere);
    }

    // Serialization round-trip preserves shape.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Orb");
        auto mesh = std::make_unique<MeshRenderer>();
        mesh->shape = scene::MeshShape::Sphere;
        mesh->color = {0.1f, 0.2f, 0.3f, 0.4f};
        obj->addComponent(std::move(mesh));

        const char* path = "build/tests/sphere_scene.scene";
        std::string error;
        assert(SceneSerializer::save(scene, path, error));
        assert(std::filesystem::exists(path));

        std::ifstream in(path);
        std::string content((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
        assert(content.find("\"mesh\": \"sphere\"") != std::string::npos);

        Scene loaded;
        loaded.createCamera({0.0f, 0.0f, 0.0f});
        assert(SceneSerializer::load(loaded, path, error));

        GameObject* loadedObj = nullptr;
        for (const auto& o : loaded.objects()) {
            if (!loaded.isCamera(o.get())) loadedObj = o.get();
        }
        assert(loadedObj != nullptr);
        MeshRenderer* loadedMesh = loadedObj->getComponent<MeshRenderer>();
        assert(loadedMesh != nullptr);
        assert(loadedMesh->shape == scene::MeshShape::Sphere);
        assert(loadedMesh->color.x == 0.1f);
        assert(loadedMesh->color.y == 0.2f);
        assert(loadedMesh->color.z == 0.3f);
        assert(loadedMesh->color.w == 0.4f);

        std::filesystem::remove(path);
    }

    // Serialization backward compatibility: missing "mesh" defaults to cube.
    {
        const char* path = "build/tests/no_mesh_field.scene";
        std::ofstream out(path);
        out << R"({"objects": [{"name": "Box", "position": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1], "components": [{"type": "MeshRenderer", "color": [1,1,1,1]}]}]})";
        out.close();

        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        std::string error;
        assert(SceneSerializer::load(scene, path, error));

        GameObject* obj = nullptr;
        for (const auto& o : scene.objects()) {
            if (!scene.isCamera(o.get())) obj = o.get();
        }
        assert(obj != nullptr);
        MeshRenderer* mesh = obj->getComponent<MeshRenderer>();
        assert(mesh != nullptr);
        assert(mesh->shape == scene::MeshShape::Cube);

        std::filesystem::remove(path);
    }

    // Serialization: unknown mesh value fails to load.
    {
        const char* path = "build/tests/unknown_mesh.scene";
        std::ofstream out(path);
        out << R"({"objects": [{"name": "Box", "position": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1], "components": [{"type": "MeshRenderer", "mesh": "teapot", "color": [1,1,1,1]}]}]})";
        out.close();

        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        std::string error;
        assert(!SceneSerializer::load(scene, path, error));
        assert(error.find("Unknown mesh shape: teapot") != std::string::npos);

        std::filesystem::remove(path);
    }

    // component.set_mesh command.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Box");
        obj->addComponent(std::make_unique<MeshRenderer>());

        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        AgentCommandResult result = executeCommand("component.set_mesh Box sphere", ctx);
        assert(result.success);
        assert(obj->getComponent<MeshRenderer>()->shape == scene::MeshShape::Sphere);

        // Missing object.
        result = executeCommand("component.set_mesh Nope sphere", ctx);
        assert(!result.success);

        // Object without MeshRenderer.
        GameObject* plain = scene.createObject("Plain");
        (void)plain;
        result = executeCommand("component.set_mesh Plain cube", ctx);
        assert(!result.success);

        // Unknown shape.
        result = executeCommand("component.set_mesh Box teapot", ctx);
        assert(!result.success);
    }

    // assert.mesh command.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Box");
        obj->addComponent(std::make_unique<MeshRenderer>());

        GameObject* selected = nullptr;
        std::string lastFailure;
        AgentCommandContext ctx{scene, selected, 0, 0.0f, 0.0f, 0, {}, nullptr, {}, {}, &lastFailure};

        AgentCommandResult result = executeCommand("assert.mesh Box cube", ctx);
        assert(result.success);
        assert(result.output == "OK");

        // Mismatch.
        lastFailure.clear();
        result = executeCommand("assert.mesh Box sphere", ctx);
        assert(!result.success);
        assert(result.output.find("Expected mesh: sphere") != std::string::npos);
        assert(result.output.find("Actual mesh:   cube") != std::string::npos);
        assert(lastFailure == result.output);

        // Missing object is an assertion failure.
        lastFailure.clear();
        result = executeCommand("assert.mesh Nope cube", ctx);
        assert(!result.success);
        assert(result.output.find("Object not found: Nope") != std::string::npos);
        assert(lastFailure == result.output);

        // Object without MeshRenderer is an assertion failure.
        GameObject* plain = scene.createObject("Plain");
        (void)plain;
        lastFailure.clear();
        result = executeCommand("assert.mesh Plain cube", ctx);
        assert(!result.success);
        assert(result.output.find("Object 'Plain' has no MeshRenderer component.") != std::string::npos);
        assert(lastFailure == result.output);

        // Unknown shape is a usage error, not an assertion failure.
        result = executeCommand("assert.mesh Box teapot", ctx);
        assert(!result.success);
        assert(result.output.find("Unknown mesh shape: teapot") != std::string::npos);
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

    // drawShape maps MeshShape to ShapeType.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});

        GameObject* cubeObj = scene.createObject("Cube");
        cubeObj->addComponent(std::make_unique<MeshRenderer>());

        GameObject* sphereObj = scene.createObject("Sphere");
        auto sphereMesh = std::make_unique<MeshRenderer>();
        sphereMesh->shape = scene::MeshShape::Sphere;
        sphereObj->addComponent(std::move(sphereMesh));

        GameObject* planeObj = scene.createObject("Plane");
        auto planeMesh = std::make_unique<MeshRenderer>();
        planeMesh->shape = scene::MeshShape::Plane;
        planeObj->addComponent(std::move(planeMesh));

        RenderContext ctx;
        scene.buildRenderCommands(ctx);

        const auto& cmds = ctx.commands();
        assert(cmds.size() == 4); // ground + cube + sphere + plane
        assert(cmds[0].type == ShapeType::Ground);
        assert(cmds[1].type == ShapeType::Cube);
        assert(cmds[2].type == ShapeType::Sphere);
        assert(cmds[3].type == ShapeType::Plane);
    }

    // New mesh commands appear in command discovery and have help.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        AgentCommandResult result = executeCommand("agent.commands", ctx);
        assert(result.success);
        assert(result.output.find("component.set_mesh") != std::string::npos);
        assert(result.output.find("assert.mesh") != std::string::npos);

        result = executeCommand("agent.help component.set_mesh", ctx);
        assert(result.success);
        assert(result.output.find("component.set_mesh <name> <shape>") != std::string::npos);

        result = executeCommand("agent.help assert.mesh", ctx);
        assert(result.success);
        assert(result.output.find("assert.mesh <name> <shape>") != std::string::npos);
    }

    // ===== Milestone 0.6: Transform Hierarchy =====

    // setParent basics + invariants.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* a = scene.createObject("A");
        GameObject* b = scene.createObject("B");

        assert(scene.setParent(a, b));
        assert(a->parent() == b);
        assert(b->children().size() == 1);
        assert(b->children()[0] == a);
        assert(a->children().empty());
    }

    // Idempotent re-parenting to the same parent is a success no-op.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* a = scene.createObject("A");
        GameObject* b = scene.createObject("B");

        assert(scene.setParent(a, b));
        assert(scene.setParent(a, b));
        assert(b->children().size() == 1);
        assert(b->children()[0] == a);
    }

    // detach / setParent(child, nullptr).
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* a = scene.createObject("A");
        GameObject* b = scene.createObject("B");

        assert(scene.setParent(a, b));
        assert(scene.setParent(a, nullptr));
        assert(a->parent() == nullptr);
        assert(b->children().empty());
    }

    // Cycle guard: after A under B, parenting B under A is rejected and unchanged.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* a = scene.createObject("A");
        GameObject* b = scene.createObject("B");

        assert(scene.setParent(a, b));
        assert(!scene.setParent(b, a));
        // Hierarchy unchanged.
        assert(a->parent() == b);
        assert(b->parent() == nullptr);
        assert(b->children().size() == 1);
        assert(b->children()[0] == a);
    }

    // Deeper cycle guard: A->B->C, then C under A rejected.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* a = scene.createObject("A");
        GameObject* b = scene.createObject("B");
        GameObject* c = scene.createObject("C");

        assert(scene.setParent(b, a));   // a -> b
        assert(scene.setParent(c, b));   // a -> b -> c
        assert(!scene.setParent(a, c)); // would cycle
        assert(a->parent() == nullptr);
    }

    // Camera guard: camera cannot be parent or child.
    {
        Scene scene;
        Camera* cam = scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* x = scene.createObject("X");

        assert(!scene.setParent(cam, x));
        assert(!scene.setParent(x, cam));
        assert(cam->parent() == nullptr);
        assert(cam->children().empty());
        assert(x->parent() == nullptr);
    }

    // updateWorldTransforms: parent translates, child local offset composes.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* parent = scene.createObject("Parent");
        GameObject* child = scene.createObject("Child");
        parent->transform().position = {2.0f, 0.0f, 0.0f};
        child->transform().position = {1.0f, 0.0f, 0.0f};
        assert(scene.setParent(child, parent));

        scene.updateWorldTransforms();
        const Mat4& wm = child->transform().worldMatrix();
        assert(std::fabs(wm.columns[3][0] - 3.0f) < 1e-4f);
        assert(std::fabs(wm.columns[3][1] - 0.0f) < 1e-4f);
        assert(std::fabs(wm.columns[3][2] - 0.0f) < 1e-4f);
    }

    // Reparenting violates objects_ order (child A has a lower id than parent B)
    // but DFS-from-roots still computes the correct world transform.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* a = scene.createObject("A"); // id 2
        GameObject* b = scene.createObject("B"); // id 3
        b->transform().position = {2.0f, 0.0f, 0.0f};
        a->transform().position = {1.0f, 0.0f, 0.0f};
        assert(scene.setParent(a, b)); // A (lower id) is child of B

        scene.updateWorldTransforms();
        const Mat4& wm = a->transform().worldMatrix();
        assert(std::fabs(wm.columns[3][0] - 3.0f) < 1e-4f);
    }

    // RotateComponent on a parent orbits a child's world position.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* pivot = scene.createObject("Pivot");
        GameObject* arm = scene.createObject("Arm");
        arm->transform().position = {1.0f, 0.0f, 0.0f};
        assert(scene.setParent(arm, pivot));

        auto rot = std::make_unique<scene::RotateComponent>();
        rot->angularVelocityEuler = {0.0f, 90.0f, 0.0f};
        pivot->addComponent(std::move(rot));

        scene.update(1.0f); // parent rotates 90° about Y; world refreshes after
        const Mat4& wm = arm->transform().worldMatrix();
        // Ry(90°) * (1,0,0) = (0,0,1)
        assert(std::fabs(wm.columns[3][0] - 0.0f) < 1e-4f);
        assert(std::fabs(wm.columns[3][1] - 0.0f) < 1e-4f);
        assert(std::fabs(wm.columns[3][2] - 1.0f) < 1e-4f);
    }

    // deleteObject cascades over the whole subtree.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* parent = scene.createObject("Parent");
        GameObject* child = scene.createObject("Child");
        GameObject* grand = scene.createObject("Grand");
        scene.setParent(child, parent);
        scene.setParent(grand, child);
        assert(scene.objects().size() == 4); // camera + 3

        assert(scene.deleteObject(parent));
        assert(scene.objects().size() == 1); // camera only
        assert(scene.findObjectById(parent->id().value) == nullptr);
        assert(scene.findObjectById(child->id().value) == nullptr);
        assert(scene.findObjectById(grand->id().value) == nullptr);
    }

    // duplicateObject cascades the subtree.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* parent = scene.createObject("Parent");
        parent->transform().position = {2.0f, 0.0f, 0.0f};
        parent->addComponent(std::make_unique<MeshRenderer>());
        GameObject* child = scene.createObject("Child");
        child->transform().position = {1.0f, 0.0f, 0.0f};
        scene.setParent(child, parent);

        GameObject* copy = scene.duplicateObject(parent);
        assert(copy != nullptr);
        assert(copy->name() == "Parent Copy");
        assert(copy->transform().position.x == 2.0f);
        assert(copy->hasComponent<MeshRenderer>());
        assert(copy->children().size() == 1);
        GameObject* copiedChild = copy->children()[0];
        assert(copiedChild->name() == "Child"); // child name preserved
        assert(copiedChild->parent() == copy);
        assert(copiedChild->transform().position.x == 1.0f);
        assert(scene.objects().size() == 5); // camera + 2 parents + 2 children
    }

    // Serialization round-trip with nested children.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* parent = scene.createObject("Parent");
        parent->transform().position = {5.0f, 0.0f, 0.0f};
        parent->addComponent(std::make_unique<MeshRenderer>());
        GameObject* child = scene.createObject("Child");
        child->transform().position = {1.0f, 0.0f, 0.0f};
        scene.setParent(child, parent);

        const char* path = "build/tests/hierarchy_scene.scene";
        std::string error;
        assert(SceneSerializer::save(scene, path, error));

        Scene loaded;
        loaded.createCamera({0.0f, 0.0f, 0.0f});
        assert(SceneSerializer::load(loaded, path, error));
        assert(loaded.objects().size() == 3); // camera + parent + child

        // Find parent and child by name in the loaded scene.
        GameObject* lparent = nullptr;
        GameObject* lchild = nullptr;
        for (const auto& o : loaded.objects()) {
            if (o->name() == "Parent") lparent = o.get();
            if (o->name() == "Child") lchild = o.get();
        }
        assert(lparent != nullptr);
        assert(lchild != nullptr);
        assert(lchild->parent() == lparent);
        assert(lparent->children().size() == 1);
        assert(lparent->children()[0] == lchild);
        assert(lparent->transform().position.x == 5.0f);
        assert(lchild->transform().position.x == 1.0f);
        assert(lparent->hasComponent<MeshRenderer>());

        // World position composes after load.
        loaded.updateWorldTransforms();
        const Mat4& wm = lchild->transform().worldMatrix();
        assert(std::fabs(wm.columns[3][0] - 6.0f) < 1e-4f);

        std::filesystem::remove(path);
    }

    // Serialization backward compatibility: a flat scene writes no "children" field.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* a = scene.createObject("Solo");
        a->transform().position = {1.0f, 2.0f, 3.0f};

        const char* path = "build/tests/flat_scene.scene";
        std::string error;
        assert(SceneSerializer::save(scene, path, error));

        std::ifstream in(path);
        std::string content((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
        assert(content.find("children") == std::string::npos);
        assert(content.find("\"name\": \"Solo\"") != std::string::npos);

        std::filesystem::remove(path);
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

    // scene.set_parent command: success + each documented error.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        scene.createObject("A");
        scene.createObject("B");
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        AgentCommandResult result = executeCommand("scene.set_parent A B", ctx);
        assert(result.success);
        assert(result.output.find("Parented") != std::string::npos);
        assert(scene.findObjectById(2)->parent() == scene.findObjectById(3));

        // Missing objects.
        result = executeCommand("scene.set_parent Nope B", ctx);
        assert(!result.success);
        result = executeCommand("scene.set_parent A Nope", ctx);
        assert(!result.success);

        // Same object.
        result = executeCommand("scene.set_parent A A", ctx);
        assert(!result.success);

        // Cycle: B is under... create C, A->B, B->C, then C->A must fail.
        scene.createObject("C");
        executeCommand("scene.set_parent B C", ctx); // A under B under C
        result = executeCommand("scene.set_parent C A", ctx);
        assert(!result.success);
        assert(result.output.find("cycle") != std::string::npos);

        // Camera as parent / child.
        result = executeCommand("scene.set_parent A Camera", ctx);
        assert(!result.success);
        result = executeCommand("scene.set_parent Camera A", ctx);
        assert(!result.success);
    }

    // scene.detach, scene.get_parent, scene.get_children commands.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* parent = scene.createObject("Parent");
        GameObject* child = scene.createObject("Child");
        scene.setParent(child, parent);
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        // get_parent (named).
        AgentCommandResult result = executeCommand("scene.get_parent Child", ctx);
        assert(result.success);
        assert(result.output.find("Parent: Parent") != std::string::npos);
        result = executeCommand("scene.get_parent Parent", ctx);
        assert(result.success);
        assert(result.output == "Parent: none");

        // get_children.
        result = executeCommand("scene.get_children Parent", ctx);
        assert(result.success);
        assert(result.output.find("Child") != std::string::npos);
        result = executeCommand("scene.get_children Child", ctx);
        assert(result.success);
        assert(result.output.find("Children: none") != std::string::npos);

        // detach.
        result = executeCommand("scene.detach Child", ctx);
        assert(result.success);
        assert(child->parent() == nullptr);
        assert(parent->children().empty());

        // detach a root is a no-op success.
        result = executeCommand("scene.detach Child", ctx);
        assert(result.success);
        assert(result.output.find("already a root") != std::string::npos);

        // Errors.
        result = executeCommand("scene.detach Nope", ctx);
        assert(!result.success);
        result = executeCommand("scene.detach Camera", ctx);
        assert(!result.success);
        result = executeCommand("scene.get_parent Nope", ctx);
        assert(!result.success);
    }

    // assert.parent: matching, none, and failure.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* parent = scene.createObject("Parent");
        GameObject* child = scene.createObject("Child");
        scene.setParent(child, parent);
        GameObject* selected = nullptr;
        std::string lastFailure;
        AgentCommandContext ctx{scene, selected, 0, 0.0f, 0.0f, 0, {}, nullptr, {}, {}, &lastFailure};

        AgentCommandResult result = executeCommand("assert.parent Child Parent", ctx);
        assert(result.success);
        assert(result.output == "OK");

        result = executeCommand("assert.parent Parent none", ctx);
        assert(result.success);

        // Mismatch (named).
        lastFailure.clear();
        result = executeCommand("assert.parent Child none", ctx);
        assert(!result.success);
        assert(result.output.find("Expected parent: none") != std::string::npos);
        assert(lastFailure == result.output);

        // Mismatch (none vs actual).
        lastFailure.clear();
        result = executeCommand("assert.parent Parent Child", ctx);
        assert(!result.success);
        assert(result.output.find("Actual parent: none") != std::string::npos);

        // Missing object is an assertion failure.
        lastFailure.clear();
        result = executeCommand("assert.parent Nope Parent", ctx);
        assert(!result.success);
        assert(result.output.find("Object not found: Nope") != std::string::npos);
    }

    // assert.world_position: success, failure, and composition.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* parent = scene.createObject("Parent");
        GameObject* child = scene.createObject("Child");
        parent->transform().position = {2.0f, 0.0f, 0.0f};
        child->transform().position = {1.0f, 0.0f, 0.0f};
        scene.setParent(child, parent); // world x = 3
        GameObject* selected = nullptr;
        std::string lastFailure;
        AgentCommandContext ctx{scene, selected, 0, 0.0f, 0.0f, 0, {}, nullptr, {}, {}, &lastFailure};

        AgentCommandResult result = executeCommand("assert.world_position Child 3 0 0", ctx);
        assert(result.success);
        assert(result.output == "OK");

        // Failure with tolerance.
        lastFailure.clear();
        result = executeCommand("assert.world_position Child 9 0 0 0.01", ctx);
        assert(!result.success);
        assert(result.output.find("Expected world position: 9.0000") != std::string::npos);
        assert(result.output.find("Actual world position:   3.0000") != std::string::npos);
        assert(lastFailure == result.output);

        // Loose tolerance passes.
        result = executeCommand("assert.world_position Child 3.4 0 0 1.0", ctx);
        assert(result.success);

        // Missing object is an assertion failure.
        lastFailure.clear();
        result = executeCommand("assert.world_position Nope 0 0 0", ctx);
        assert(!result.success);
        assert(result.output.find("Object not found: Nope") != std::string::npos);
    }

    // New hierarchy commands appear in command discovery and have help.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        AgentCommandResult result = executeCommand("agent.commands", ctx);
        assert(result.success);
        assert(result.output.find("scene.set_parent") != std::string::npos);
        assert(result.output.find("scene.detach") != std::string::npos);
        assert(result.output.find("scene.get_parent") != std::string::npos);
        assert(result.output.find("scene.get_children") != std::string::npos);
        assert(result.output.find("assert.parent") != std::string::npos);
        assert(result.output.find("assert.world_position") != std::string::npos);

        result = executeCommand("agent.help scene.set_parent", ctx);
        assert(result.success);
        assert(result.output.find("scene.set_parent <childName> <parentName>") != std::string::npos);

        result = executeCommand("agent.help assert.world_position", ctx);
        assert(result.success);
        assert(result.output.find("assert.world_position <name>") != std::string::npos);
    }

    std::printf("All tests passed.\n");
    return 0;
}
