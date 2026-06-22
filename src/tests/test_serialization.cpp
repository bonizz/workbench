#include "scene/camera.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
#include "scene/rotate_component.h"
#include "scene/scene.h"
#include "scene/scene_document.h"
#include "scene/scene_io.h"
#include "scene/scene_serializer.h"
#include "core/settings.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using scene::MeshRenderer;

void runTestSerialization()
{
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

    // Camera round-trip.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        scene.camera().transform().position = {1.0f, 2.0f, 3.0f};
        scene.camera().transform().rotation = {0.1f, 0.2f, 0.3f};
        scene.camera().setMoveSpeed(7.5f);

        const char* path = "build/tests/camera_scene.scene";
        std::string error;
        assert(SceneSerializer::save(scene, path, error));

        Scene loaded;
        loaded.createCamera({0.0f, 0.0f, 0.0f});
        assert(SceneSerializer::load(loaded, path, error));

        assert(loaded.camera().transform().position.x == 1.0f);
        assert(loaded.camera().transform().position.y == 2.0f);
        assert(loaded.camera().transform().position.z == 3.0f);
        assert(loaded.camera().transform().rotation.x == 0.1f);
        assert(loaded.camera().transform().rotation.y == 0.2f);
        assert(loaded.camera().transform().rotation.z == 0.3f);
        assert(loaded.camera().moveSpeed() == 7.5f);

        std::filesystem::remove(path);
    }

    // Environment round-trip.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        scene.environment().light.direction = {-0.1f, -0.2f, -0.3f};
        scene.environment().light.ambient = 0.25f;
        scene.environment().light.diffuse = 0.75f;
        scene.environment().sky.horizonColor = {0.1f, 0.2f, 0.3f};
        scene.environment().sky.zenithColor = {0.4f, 0.5f, 0.6f};
        scene.environment().sky.sunColor = {0.7f, 0.8f, 0.9f};
        scene.environment().sky.sunSize = 0.02f;
        scene.environment().sky.sunIntensity = 2.0f;

        const char* path = "build/tests/environment_scene.scene";
        std::string error;
        assert(SceneSerializer::save(scene, path, error));

        Scene loaded;
        loaded.createCamera({0.0f, 0.0f, 0.0f});
        assert(SceneSerializer::load(loaded, path, error));

        assert(loaded.environment().light.direction.x == -0.1f);
        assert(loaded.environment().light.direction.y == -0.2f);
        assert(loaded.environment().light.direction.z == -0.3f);
        assert(loaded.environment().light.ambient == 0.25f);
        assert(loaded.environment().light.diffuse == 0.75f);
        assert(loaded.environment().sky.horizonColor.x == 0.1f);
        assert(loaded.environment().sky.zenithColor.y == 0.5f);
        assert(loaded.environment().sky.sunColor.z == 0.9f);
        assert(loaded.environment().sky.sunSize == 0.02f);
        assert(loaded.environment().sky.sunIntensity == 2.0f);

        std::filesystem::remove(path);
    }

    // Missing version defaults to 1.
    {
        const char* path = "build/tests/no_version.scene";
        std::ofstream out(path);
        out << R"({"objects": []})";
        out.close();

        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        std::string error;
        assert(SceneSerializer::load(scene, path, error));

        std::filesystem::remove(path);
    }

    // Missing camera/environment/settings blocks apply defaults.
    {
        const char* path = "build/tests/minimal_header.scene";
        std::ofstream out(path);
        out << R"({"version": 1, "objects": [{"name": "Only", "position": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1], "components": [{"type": "MeshRenderer", "mesh": "cube", "color": [1,1,1,1]}]}]})";
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
        assert(obj->name() == "Only");
        assert(scene.camera().transform().position.x == 0.0f);
        assert(scene.environment().light.ambient == 0.15f);

        std::filesystem::remove(path);
    }

    // Unknown higher version loads with a warning.
    {
        const char* path = "build/tests/future_version.scene";
        std::ofstream out(path);
        out << R"({"version": 99, "objects": []})";
        out.close();

        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        std::string error;
        std::string warning;
        assert(SceneSerializer::load(scene, path, error, &warning));
        assert(!warning.empty());
        assert(warning.find("Unknown scene version") != std::string::npos);

        std::filesystem::remove(path);
    }

    // Loading a scene does not touch app settings.
    {
        const char* settingsPath = "build/tests/app_settings.json";
        Settings::setSettingsPath(settingsPath);
        std::filesystem::remove(settingsPath);

        // Seed app settings with window/editor values.
        Settings::saveWindowSize(123, 456);
        std::unordered_map<std::string, bool> states = {{"Hierarchy", false}};
        Settings::saveEditorWindowStates(states);

        const char* path = "build/tests/settings_unchanged.scene";
        std::ofstream out(path);
        out << R"({"objects": [{"name": "X", "position": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1], "components": [{"type": "MeshRenderer", "mesh": "cube", "color": [1,1,1,1]}]}]})";
        out.close();

        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        std::string error;
        assert(SceneSerializer::load(scene, path, error));

        int width = 0, height = 0;
        assert(Settings::loadWindowSize(width, height));
        assert(width == 123);
        assert(height == 456);

        std::unordered_map<std::string, bool> loadedStates;
        assert(Settings::loadEditorWindowStates(loadedStates));
        assert(loadedStates["Hierarchy"] == false);

        std::filesystem::remove(path);
        std::filesystem::remove(settingsPath);
    }

    // SceneIO::resolveScenePath appends the extension when absent.
    {
        std::string error;
        assert(SceneIO::resolveScenePath("arena", error) == "assets/scenes/arena.scene.json");
        assert(error.empty());
    }

    // SceneIO::resolveScenePath leaves an existing extension untouched.
    {
        std::string error;
        assert(SceneIO::resolveScenePath("arena.scene.json", error) ==
               "assets/scenes/arena.scene.json");
        assert(error.empty());
    }

    // SceneIO::resolveScenePath trims surrounding whitespace.
    {
        std::string error;
        assert(SceneIO::resolveScenePath("  arena  ", error) ==
               "assets/scenes/arena.scene.json");
    }

    // SceneIO::resolveScenePath rejects empty / whitespace-only names.
    {
        std::string error;
        assert(SceneIO::resolveScenePath("", error).empty());
        assert(!error.empty());
        error.clear();
        assert(SceneIO::resolveScenePath("   ", error).empty());
        assert(!error.empty());
    }

    // SceneIO::resolveScenePath rejects path separators and parent traversal.
    {
        std::string error;
        assert(SceneIO::resolveScenePath("sub/arena", error).empty());
        assert(!error.empty());
        error.clear();
        assert(SceneIO::resolveScenePath("a\\b", error).empty());
        assert(!error.empty());
        error.clear();
        assert(SceneIO::resolveScenePath("../escape", error).empty());
        assert(!error.empty());
    }

    // SceneIO::exists reflects the resolved path on disk.
    {
        std::filesystem::create_directories("assets/scenes");
        const char* path = "assets/scenes/_io_probe.scene.json";
        std::filesystem::remove(path);
        assert(!SceneIO::exists("_io_probe"));

        std::ofstream out(path);
        out << "{}";
        out.close();
        assert(SceneIO::exists("_io_probe"));
        assert(SceneIO::exists("_io_probe.scene.json"));

        std::filesystem::remove(path);
    }

    // SceneIO::listScenes returns sorted, project-relative .scene.json paths and
    // ignores non-scene files.
    {
        std::filesystem::create_directories("assets/scenes");
        const char* a = "assets/scenes/_list_b.scene.json";
        const char* b = "assets/scenes/_list_a.scene.json";
        const char* other = "assets/scenes/_list_note.txt";
        for (const char* p : {a, b, other}) {
            std::ofstream(p) << "{}";
        }

        std::vector<std::string> scenes = SceneIO::listScenes();
        auto has = [&](const std::string& path) {
            return std::find(scenes.begin(), scenes.end(), path) != scenes.end();
        };
        assert(has("assets/scenes/_list_a.scene.json"));
        assert(has("assets/scenes/_list_b.scene.json"));
        assert(!has(other));
        // Output is sorted: _list_a precedes _list_b.
        auto ia = std::find(scenes.begin(), scenes.end(), "assets/scenes/_list_a.scene.json");
        auto ib = std::find(scenes.begin(), scenes.end(), "assets/scenes/_list_b.scene.json");
        assert(ia < ib);

        std::filesystem::remove(a);
        std::filesystem::remove(b);
        std::filesystem::remove(other);
    }

    // Settings recent-scenes list: front insertion, dedup, and cap.
    {
        const std::string settingsPath = "test_recent_settings.json";
        std::filesystem::remove(settingsPath);
        Settings::setSettingsPath(settingsPath);

        std::vector<std::string> empty;
        assert(!Settings::loadRecentScenes(empty));

        Settings::addRecentScene("assets/scenes/one.scene.json");
        Settings::addRecentScene("assets/scenes/two.scene.json");
        Settings::addRecentScene("assets/scenes/three.scene.json");

        std::vector<std::string> recent;
        assert(Settings::loadRecentScenes(recent));
        // Most-recent first.
        assert(recent.size() == 3);
        assert(recent[0] == "assets/scenes/three.scene.json");
        assert(recent[1] == "assets/scenes/two.scene.json");
        assert(recent[2] == "assets/scenes/one.scene.json");

        // Re-adding an existing path moves it to the front without duplicating.
        Settings::addRecentScene("assets/scenes/one.scene.json");
        recent.clear();
        Settings::loadRecentScenes(recent);
        assert(recent.size() == 3);
        assert(recent[0] == "assets/scenes/one.scene.json");
        assert(recent[1] == "assets/scenes/three.scene.json");
        assert(recent[2] == "assets/scenes/two.scene.json");

        // Cap at kMaxRecentScenes: add more unique entries than the limit.
        for (int i = 0; i < 15; ++i) {
            Settings::addRecentScene("assets/scenes/s" + std::to_string(i) + ".scene.json");
        }
        recent.clear();
        Settings::loadRecentScenes(recent);
        assert(recent.size() == Settings::kMaxRecentScenes);
        // The newest addition is at the front.
        assert(recent[0] == "assets/scenes/s14.scene.json");

        Settings::setSettingsPath("settings.json");
        std::filesystem::remove(settingsPath);
    }
}
