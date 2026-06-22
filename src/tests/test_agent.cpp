#include "agent/command.h"
#include "agent/script_runner.h"
#include "core/math.h"
#include "scene/camera.h"
#include "scene/component.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
#include "scene/rotate_component.h"
#include "scene/scene.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using scene::MeshRenderer;

void runTestAgent()
{
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
        assert(scene.loadedScenePath() == "assets/scenes/test_cmd.scene");

        selected = scene.findObjectById(id);
        assert(selected != nullptr);
        result = executeCommand("scene.load test_cmd.scene", ctx);
        assert(result.success);
        assert(selected == nullptr);
        assert(scene.objects().size() == 2);

        std::filesystem::remove("assets/scenes/test_cmd.scene");
    }

    // scene.create_primitive command (generalized creator for cube/sphere/plane).
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        // Default name = capitalized shape; selection moves to it.
        AgentCommandResult result = executeCommand("scene.create_primitive sphere", ctx);
        assert(result.success);
        assert(selected != nullptr);
        assert(selected->name() == "Sphere");
        assert(selected->getComponent<MeshRenderer>()->shape == scene::MeshShape::Sphere);

        // Explicit name + shape.
        result = executeCommand("scene.create_primitive plane Flatty", ctx);
        assert(result.success);
        assert(selected->name() == "Flatty");
        assert(selected->getComponent<MeshRenderer>()->shape == scene::MeshShape::Plane);

        // Missing shape argument.
        result = executeCommand("scene.create_primitive", ctx);
        assert(!result.success);

        // Unknown shape.
        result = executeCommand("scene.create_primitive teapot", ctx);
        assert(!result.success);
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
        assert(result.output.find("scene.create_primitive") != std::string::npos);
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
}
