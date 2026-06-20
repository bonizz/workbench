#include "agent/command.h"
#include "agent/script_runner.h"
#include "agent/test_suite.h"
#include "capture/capture.h"
#include "core/cli_options.h"
#include "core/math.h"
#include "core/object_id.h"
#include "debug/bundle.h"
#include "debug/debug_state.h"
#include "renderer/render_context.h"
#include "scene/camera.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "scene/transform.h"

#include <cassert>
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
        ctx.drawCube(identity(), {1.0f, 0.0f, 0.0f, 1.0f});
        ctx.drawCube(identity(), {0.0f, 1.0f, 0.0f, 1.0f});
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

    std::printf("All tests passed.\n");
    return 0;
}
