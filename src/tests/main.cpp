#include "agent/command.h"
#include "core/math.h"
#include "core/object_id.h"
#include "debug/debug_state.h"
#include "renderer/render_context.h"
#include "scene/camera.h"
#include "scene/game_object.h"
#include "scene/scene.h"
#include "scene/transform.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

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

    std::printf("All tests passed.\n");
    return 0;
}
