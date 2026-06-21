#include "agent/command.h"
#include "core/math.h"
#include "scene/camera.h"
#include "scene/component.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
#include "scene/rotate_component.h"
#include "scene/scene.h"
#include "scene/transform.h"

#include <cassert>
#include <cmath>
#include <memory>
#include <string>

using scene::MeshRenderer;

void runTestScene()
{
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
}
