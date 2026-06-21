#include "core/cli_options.h"
#include "core/input_state.h"
#include "core/math.h"
#include "core/object_id.h"
#include "core/settings.h"
#include "scene/camera.h"
#include "scene/game_object.h"
#include "scene/scene.h"
#include "scene/transform.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

void runTestCore()
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

    // Settings: window size round-trip.
    {
        struct SettingsPathGuard {
            std::string previous;
            std::string path;
            SettingsPathGuard(const std::string& p) : previous("settings.json"), path(p) {
                Settings::setSettingsPath(p);
            }
            ~SettingsPathGuard() {
                Settings::setSettingsPath(previous);
                std::filesystem::remove(path);
            }
        };

        const char* path = "build/tests/test_settings.json";
        SettingsPathGuard guard(path);
        (void)guard;

        Settings::saveWindowSize(1024, 768);

        int width = 0;
        int height = 0;
        assert(Settings::loadWindowSize(width, height));
        assert(width == 1024);
        assert(height == 768);
    }

    // Settings: window position round-trip.
    {
        struct SettingsPathGuard {
            std::string previous;
            std::string path;
            SettingsPathGuard(const std::string& p) : previous("settings.json"), path(p) {
                Settings::setSettingsPath(p);
            }
            ~SettingsPathGuard() {
                Settings::setSettingsPath(previous);
                std::filesystem::remove(path);
            }
        };

        const char* path = "build/tests/test_position.json";
        SettingsPathGuard guard(path);
        (void)guard;

        Settings::saveWindowPosition(120, 240);

        int x = 0;
        int y = 0;
        assert(Settings::loadWindowPosition(x, y));
        assert(x == 120);
        assert(y == 240);
    }

    // Settings: editor window state round-trip and preservation.
    {
        struct SettingsPathGuard {
            std::string previous;
            std::string path;
            SettingsPathGuard(const std::string& p) : previous("settings.json"), path(p) {
                Settings::setSettingsPath(p);
            }
            ~SettingsPathGuard() {
                Settings::setSettingsPath(previous);
                std::filesystem::remove(path);
            }
        };

        const char* path = "build/tests/test_editor_settings.json";
        SettingsPathGuard guard(path);
        (void)guard;

        Settings::saveWindowSize(800, 600);
        Settings::saveEditorWindowStates({{"Hierarchy", true}, {"Inspector", false}, {"AgentConsole", true}});

        int width = 0;
        int height = 0;
        assert(Settings::loadWindowSize(width, height));
        assert(width == 800);
        assert(height == 600);

        std::unordered_map<std::string, bool> states;
        assert(Settings::loadEditorWindowStates(states));
        assert(states.size() == 3);
        assert(states["Hierarchy"] == true);
        assert(states["Inspector"] == false);
        assert(states["AgentConsole"] == true);

        // The file should be JSON with both window and editor sections.
        {
            std::ifstream in(path);
            std::string content((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
            assert(content.find("\"window\"") != std::string::npos);
            assert(content.find("\"editor\"") != std::string::npos);
            assert(content.find("\"show_Hierarchy\"") != std::string::npos);
            assert(content.find("\"show_Inspector\"") != std::string::npos);
        }

        // Updating the window size must keep the editor state keys.
        Settings::saveWindowSize(1280, 720);
        states.clear();
        assert(Settings::loadEditorWindowStates(states));
        assert(states.size() == 3);
        assert(states["Hierarchy"] == true);
    }

    // Settings: camera state round-trip.
    {
        struct SettingsPathGuard {
            std::string previous;
            std::string path;
            SettingsPathGuard(const std::string& p) : previous("settings.json"), path(p) {
                Settings::setSettingsPath(p);
            }
            ~SettingsPathGuard() {
                Settings::setSettingsPath(previous);
                std::filesystem::remove(path);
            }
        };

        const char* path = "build/tests/test_camera_settings.json";
        SettingsPathGuard guard(path);
        (void)guard;

        Vec3 position = {1.0f, 2.0f, 3.0f};
        Vec3 rotation = {0.1f, 0.2f, 0.3f};
        float moveSpeed = 12.5f;
        Settings::saveCamera(position, rotation, moveSpeed);

        Vec3 loadedPosition = {};
        Vec3 loadedRotation = {};
        float loadedSpeed = 0.0f;
        assert(Settings::loadCamera(loadedPosition, loadedRotation, loadedSpeed));
        assert(loadedPosition.x == position.x);
        assert(loadedPosition.y == position.y);
        assert(loadedPosition.z == position.z);
        assert(loadedRotation.x == rotation.x);
        assert(loadedRotation.y == rotation.y);
        assert(loadedRotation.z == rotation.z);
        assert(loadedSpeed == moveSpeed);
    }

    // Settings: lighting + sky round-trip.
    {
        struct SettingsPathGuard {
            std::string previous;
            std::string path;
            SettingsPathGuard(const std::string& p) : previous("settings.json"), path(p) {
                Settings::setSettingsPath(p);
            }
            ~SettingsPathGuard() {
                Settings::setSettingsPath(previous);
                std::filesystem::remove(path);
            }
        };

        const char* path = "build/tests/test_lighting_settings.json";
        SettingsPathGuard guard(path);
        (void)guard;

        LightSettings light;
        light.direction = {-0.5f, -0.8f, -0.2f};
        light.ambient = 0.42f;
        light.diffuse = 1.3f;

        SkySettings sky;
        sky.horizonColor = {0.9f, 0.5f, 0.3f};
        sky.zenithColor = {0.1f, 0.15f, 0.4f};
        sky.sunColor = {1.0f, 0.7f, 0.4f};
        sky.sunSize = 0.022f;
        sky.sunIntensity = 1.6f;

        Settings::saveLighting(light, sky);

        LightSettings loadedLight;
        SkySettings loadedSky;
        assert(Settings::loadLighting(loadedLight, loadedSky));
        assert(loadedLight.direction.x == light.direction.x);
        assert(loadedLight.direction.y == light.direction.y);
        assert(loadedLight.direction.z == light.direction.z);
        assert(loadedLight.ambient == light.ambient);
        assert(loadedLight.diffuse == light.diffuse);
        assert(loadedSky.horizonColor.x == sky.horizonColor.x);
        assert(loadedSky.zenithColor.z == sky.zenithColor.z);
        assert(loadedSky.sunColor.y == sky.sunColor.y);
        assert(loadedSky.sunSize == sky.sunSize);
        assert(loadedSky.sunIntensity == sky.sunIntensity);
    }

    // Settings: missing lighting fields fall back to struct defaults.
    {
        struct SettingsPathGuard {
            std::string previous;
            std::string path;
            SettingsPathGuard(const std::string& p) : previous("settings.json"), path(p) {
                Settings::setSettingsPath(p);
            }
            ~SettingsPathGuard() {
                Settings::setSettingsPath(previous);
                std::filesystem::remove(path);
            }
        };

        const char* path = "build/tests/test_lighting_partial.json";
        SettingsPathGuard guard(path);
        (void)guard;

        // A settings file from an older build: a "lighting" block that only
        // carries one sky field. Everything else must keep its default.
        {
            std::ofstream out(path);
            out << "{\n  \"lighting\": {\n    \"sky\": { \"sunSize\": 0.033 }\n  }\n}\n";
        }

        LightSettings defaultLight;
        SkySettings defaultSky;

        LightSettings loadedLight;
        SkySettings loadedSky;
        assert(Settings::loadLighting(loadedLight, loadedSky));

        // Present field applied.
        assert(loadedSky.sunSize == 0.033f);
        // Absent fields preserved at defaults.
        assert(loadedSky.sunIntensity == defaultSky.sunIntensity);
        assert(loadedSky.horizonColor.x == defaultSky.horizonColor.x);
        assert(loadedLight.direction.y == defaultLight.direction.y);
        assert(loadedLight.ambient == defaultLight.ambient);
        assert(loadedLight.diffuse == defaultLight.diffuse);
    }

    // Camera: reset restores the default position, rotation, and speed.
    {
        Scene scene;
        Camera* camera = scene.createCamera({0.0f, 0.0f, 0.0f});
        camera->transform().position = {10.0f, 20.0f, 30.0f};
        camera->transform().rotation = {0.5f, 0.6f, 0.7f};
        camera->setMoveSpeed(42.0f);

        camera->reset();

        assert(camera->transform().position.x == 0.0f);
        assert(camera->transform().position.y == 3.0f);
        assert(camera->transform().position.z == 5.0f);
        assert(camera->transform().rotation.x == 0.0f);
        assert(camera->transform().rotation.y == 0.0f);
        assert(camera->transform().rotation.z == 0.0f);
        assert(camera->moveSpeed() == 5.0f);
    }

    // View-matrix basis extraction matches the lookAt row layout.
    {
        Scene scene;
        Camera* camera = scene.createCamera({0.0f, 0.0f, 0.0f});
        camera->transform().rotation = {0.0f, 90.0f * kDegToRad, 0.0f};
        camera->update(0.0f, InputState{});

        Mat4 view = camera->viewMatrix();
        Vec3 right = viewRight(view);
        Vec3 up = viewUp(view);
        Vec3 backward = viewBackward(view);

        assert(std::fabs(right.x) < 1e-4f);
        assert(std::fabs(right.y) < 1e-4f);
        assert(std::fabs(right.z - 1.0f) < 1e-4f);

        assert(std::fabs(up.x) < 1e-4f);
        assert(std::fabs(up.y - 1.0f) < 1e-4f);
        assert(std::fabs(up.z) < 1e-4f);

        assert(std::fabs(backward.x + 1.0f) < 1e-4f);
        assert(std::fabs(backward.y) < 1e-4f);
        assert(std::fabs(backward.z) < 1e-4f);
    }
}
