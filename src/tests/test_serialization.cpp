#include "scene/camera.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
#include "scene/rotate_component.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

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
}
