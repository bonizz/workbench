#include "scene/camera.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
#include "scene/scene.h"
#include "scene/scene_history.h"
#include "scene/scene_serializer.h"

#include <cassert>
#include <cmath>
#include <memory>
#include <string>

using scene::MeshRenderer;

// Snapshots deserialize into freshly-created GameObjects (with fresh ObjectIds),
// so stale pointers are invalid after undo/redo. Re-resolve by name each time.
static GameObject* firstAuthored(Scene& scene)
{
    for (const auto& obj : scene.objects()) {
        if (!scene.isCamera(obj.get())) return obj.get();
    }
    return nullptr;
}

void runTestUndo()
{
    // 1. Create object then undo/redo.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        SceneHistory h;

        assert(scene.objects().size() == 1); // camera only
        h.push(scene);                       // checkpoint before create
        scene.createObject("Cube");
        assert(scene.objects().size() == 2);

        assert(h.undo(scene));
        assert(scene.objects().size() == 1); // create undone

        assert(h.redo(scene));
        assert(scene.objects().size() == 2); // create restored
        GameObject* obj = firstAuthored(scene);
        assert(obj != nullptr);
        assert(obj->name() == "Cube");
    }

    // 2. Transform edit then undo/redo (refetch: snapshots get fresh ids).
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Cube");
        obj->transform().position = {1.0f, 2.0f, 3.0f};

        SceneHistory h;
        h.push(scene);                       // checkpoint before transform edit
        obj->transform().position = {9.0f, 9.0f, 9.0f};

        assert(h.undo(scene));
        GameObject* restored = firstAuthored(scene);
        assert(restored != nullptr);
        assert(restored->name() == "Cube");
        assert(restored->transform().position.x == 1.0f);
        assert(restored->transform().position.y == 2.0f);
        assert(restored->transform().position.z == 3.0f);

        assert(h.redo(scene));
        GameObject* afterRedo = firstAuthored(scene);
        assert(afterRedo != nullptr);
        assert(afterRedo->transform().position.x == 9.0f);
    }

    // 3. Lighting edit then undo/redo.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        scene.environment().light.ambient = 0.5f;

        SceneHistory h;
        h.push(scene);
        scene.environment().light.ambient = 0.9f;
        assert(scene.environment().light.ambient == 0.9f);

        assert(h.undo(scene));
        assert(std::abs(scene.environment().light.ambient - 0.5f) < 1e-5f);

        assert(h.redo(scene));
        assert(std::abs(scene.environment().light.ambient - 0.9f) < 1e-5f);
    }

    // 4. Delete object then undo/redo (component + color restored).
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* obj = scene.createObject("Cube");
        auto mesh = std::make_unique<MeshRenderer>();
        mesh->color = {0.2f, 0.4f, 0.6f, 1.0f};
        obj->addComponent(std::move(mesh));

        SceneHistory h;
        h.push(scene);
        scene.deleteObject(obj);
        assert(firstAuthored(scene) == nullptr);

        assert(h.undo(scene));
        GameObject* restored = firstAuthored(scene);
        assert(restored != nullptr);
        assert(restored->name() == "Cube");
        MeshRenderer* rm = restored->getComponent<MeshRenderer>();
        assert(rm != nullptr);
        assert(std::abs(rm->color.x - 0.2f) < 1e-5f);

        assert(h.redo(scene));
        assert(firstAuthored(scene) == nullptr);
    }

    // 5. Undo stack cap (50).
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        SceneHistory h;

        // 60 checkpoints; each differs (an object is added between pushes).
        for (int i = 0; i < 60; ++i) {
            h.push(scene);
            scene.createObject("Obj" + std::to_string(i));
        }
        assert(h.undoCount() == SceneHistory::kMaxEntries); // 50; 10 oldest dropped
        assert(h.canUndo());
        assert(!h.canRedo());

        // Undo all 50; each must succeed.
        for (size_t i = 0; i < SceneHistory::kMaxEntries; ++i) {
            assert(h.undo(scene));
        }
        // 51st undo must fail (stack empty).
        assert(!h.undo(scene));
        assert(!h.canUndo());

        // After fully undoing, the scene reflects the oldest surviving
        // checkpoint (push #11), taken when Obj0..Obj9 existed (10 objects).
        // Obj10..Obj59 are gone; the 10 dropped checkpoints predated them.
        size_t authored = 0;
        for (const auto& o : scene.objects()) {
            if (!scene.isCamera(o.get())) ++authored;
        }
        assert(authored == 10);
        assert(h.redoCount() == SceneHistory::kMaxEntries);
    }

    // 6. Redo is cleared by a new push (undo, then a fresh edit kills redo).
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        SceneHistory h;

        h.push(scene);
        scene.createObject("A");
        h.push(scene);
        scene.createObject("B");
        assert(h.undo(scene)); // back to {A}
        assert(h.redoCount() == 1);

        scene.createObject("C"); // branch
        h.push(scene);
        assert(h.redoCount() == 0); // redo cleared by the new checkpoint
    }
}
