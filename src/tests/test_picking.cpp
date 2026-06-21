#include "core/input_state.h"
#include "core/math.h"
#include "core/picking.h"
#include "scene/camera.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
#include "scene/scene.h"

#include <cassert>
#include <cmath>
#include <memory>

using scene::MeshRenderer;

void runTestPicking()
{
    // ===== Milestone 0.9: Object Picking =====

    // makeCameraRay: center ray points down the camera's view direction.
    {
        Scene scene;
        Camera* camera = scene.createCamera({0.0f, 0.0f, 0.0f});
        camera->transform().rotation = {0.0f, 0.0f, 0.0f};
        camera->setAspect(1.0f);

        Ray ray = makeCameraRay(camera->transform().position,
                                camera->viewMatrix(),
                                1.0f,
                                60.0f * kDegToRad,
                                0.0f,
                                0.0f);
        // Camera at origin with zero rotation looks down -Z.
        assert(std::fabs(ray.direction.x) < 1e-4f);
        assert(std::fabs(ray.direction.y) < 1e-4f);
        assert(std::fabs(ray.direction.z + 1.0f) < 1e-4f);
    }

    // makeCameraRay: corner rays point outward within the frustum.
    {
        Scene scene;
        Camera* camera = scene.createCamera({0.0f, 0.0f, 0.0f});
        camera->setAspect(1.0f);

        Ray topRight = makeCameraRay(camera->transform().position,
                                     camera->viewMatrix(),
                                     1.0f,
                                     60.0f * kDegToRad,
                                     1.0f,
                                     1.0f);
        assert(topRight.direction.x > 0.0f);
        assert(topRight.direction.y > 0.0f);
        assert(topRight.direction.z < 0.0f);
    }

    // makeCameraRay: non-zero yaw extracts the correct world-space basis rows.
    {
        Scene scene;
        Camera* camera = scene.createCamera({0.0f, 0.0f, 0.0f});
        camera->transform().rotation = {0.0f, 90.0f * kDegToRad, 0.0f};
        camera->setAspect(1.0f);
        camera->update(0.0f, InputState{});

        Ray center = makeCameraRay(camera->transform().position,
                                   camera->viewMatrix(),
                                   1.0f,
                                   60.0f * kDegToRad,
                                   0.0f,
                                   0.0f);
        assert(std::fabs(center.direction.x - 1.0f) < 1e-4f);
        assert(std::fabs(center.direction.y) < 1e-4f);
        assert(std::fabs(center.direction.z) < 1e-4f);

        Ray rightEdge = makeCameraRay(camera->transform().position,
                                      camera->viewMatrix(),
                                      1.0f,
                                      60.0f * kDegToRad,
                                      1.0f,
                                      0.0f);
        assert(rightEdge.direction.z > 0.0f);
        assert(rightEdge.direction.x > 0.0f);
    }

    // intersectRaySphere: hit along -Z.
    {
        Vec3 origin = {0.0f, 0.0f, 5.0f};
        Vec3 dir = {0.0f, 0.0f, -1.0f};
        float t = 0.0f;
        assert(intersectRaySphere(origin, dir, 1.0f, t));
        assert(std::fabs(t - 4.0f) < 1e-4f);
    }

    // intersectRaySphere: miss.
    {
        Vec3 origin = {0.0f, 2.0f, 5.0f};
        Vec3 dir = {0.0f, 0.0f, -1.0f};
        float t = 0.0f;
        assert(!intersectRaySphere(origin, dir, 1.0f, t));
    }

    // intersectRaySphere: ray starts inside returns exit distance.
    {
        Vec3 origin = {0.0f, 0.0f, 0.0f};
        Vec3 dir = {0.0f, 0.0f, 1.0f};
        float t = 0.0f;
        assert(intersectRaySphere(origin, dir, 1.0f, t));
        assert(std::fabs(t - 1.0f) < 1e-4f);
    }

    // intersectRayAABB: hit front face of unit cube.
    {
        Vec3 origin = {0.0f, 0.0f, 5.0f};
        Vec3 dir = {0.0f, 0.0f, -1.0f};
        float t = 0.0f;
        assert(intersectRayAABB(origin, dir, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, t));
        assert(std::fabs(t - 4.0f) < 1e-4f);
    }

    // intersectRayAABB: miss outside slab.
    {
        Vec3 origin = {2.0f, 0.0f, 5.0f};
        Vec3 dir = {0.0f, 0.0f, -1.0f};
        float t = 0.0f;
        assert(!intersectRayAABB(origin, dir, {-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, t));
    }

    // intersectRayPlane: hit within bounds.
    {
        Vec3 origin = {0.0f, 1.0f, 0.0f};
        Vec3 dir = {0.0f, -1.0f, 0.0f};
        float t = 0.0f;
        assert(intersectRayPlane(origin, dir, t));
        assert(std::fabs(t - 1.0f) < 1e-4f);
    }

    // intersectRayPlane: miss due to out-of-bounds x.
    {
        Vec3 origin = {2.0f, 1.0f, 0.0f};
        Vec3 dir = {0.0f, -1.0f, 0.0f};
        float t = 0.0f;
        assert(!intersectRayPlane(origin, dir, t));
    }

    // intersectRayPlane: parallel ray misses.
    {
        Vec3 origin = {0.0f, 1.0f, 0.0f};
        Vec3 dir = {1.0f, 0.0f, 0.0f};
        float t = 0.0f;
        assert(!intersectRayPlane(origin, dir, t));
    }

    // Scene::pickObject: nearest hit along the same ray.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});

        GameObject* nearCube = scene.createObject("Near");
        nearCube->transform().position = {0.0f, 0.0f, -2.0f};
        nearCube->transform().scale = {0.5f, 0.5f, 0.5f};
        nearCube->addComponent(std::make_unique<MeshRenderer>());

        GameObject* farCube = scene.createObject("Far");
        farCube->transform().position = {0.0f, 0.0f, -5.0f};
        farCube->transform().scale = {0.5f, 0.5f, 0.5f};
        farCube->addComponent(std::make_unique<MeshRenderer>());

        Vec3 origin = {0.0f, 0.0f, 0.0f};
        Vec3 dir = {0.0f, 0.0f, -1.0f};
        GameObject* hit = scene.pickObject(origin, dir);
        assert(hit == nearCube);
    }

    // Scene::pickObject + makeCameraRay: yawed camera still hits the object in front.
    {
        Scene scene;
        Camera* camera = scene.createCamera({0.0f, 0.0f, 0.0f});
        camera->transform().rotation = {0.0f, 90.0f * kDegToRad, 0.0f};
        camera->setAspect(1.0f);
        camera->update(0.0f, InputState{});

        GameObject* cube = scene.createObject("Cube");
        cube->transform().position = {2.0f, 0.0f, 0.0f};
        cube->transform().scale = {0.5f, 0.5f, 0.5f};
        cube->addComponent(std::make_unique<MeshRenderer>());

        Ray ray = makeCameraRay(camera->transform().position,
                                camera->viewMatrix(),
                                1.0f,
                                60.0f * kDegToRad,
                                0.0f,
                                0.0f);
        assert(scene.pickObject(ray.origin, ray.direction) == cube);
    }

    // Scene::pickObject: camera is not pickable.
    {
        Scene scene;
        Camera* camera = scene.createCamera({0.0f, 0.0f, 0.0f});
        (void)camera;

        Vec3 origin = {0.0f, 0.0f, 0.0f};
        Vec3 dir = {0.0f, 0.0f, -1.0f};
        GameObject* hit = scene.pickObject(origin, dir);
        assert(hit == nullptr);
    }

    // Scene::pickObject: inactive object is not pickable.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});

        GameObject* cube = scene.createObject("Cube");
        cube->transform().position = {0.0f, 0.0f, -2.0f};
        cube->transform().scale = {0.5f, 0.5f, 0.5f};
        cube->addComponent(std::make_unique<MeshRenderer>());
        cube->setActive(false);

        Vec3 origin = {0.0f, 0.0f, 0.0f};
        Vec3 dir = {0.0f, 0.0f, -1.0f};
        GameObject* hit = scene.pickObject(origin, dir);
        assert(hit == nullptr);
    }

    // Scene::pickObject: object without MeshRenderer is not pickable.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});

        GameObject* plain = scene.createObject("Plain");
        plain->transform().position = {0.0f, 0.0f, -2.0f};
        (void)plain;

        Vec3 origin = {0.0f, 0.0f, 0.0f};
        Vec3 dir = {0.0f, 0.0f, -1.0f};
        GameObject* hit = scene.pickObject(origin, dir);
        assert(hit == nullptr);
    }

    // Scene::pickObject: sphere and plane shapes are pickable.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});

        GameObject* sphere = scene.createObject("Sphere");
        sphere->transform().position = {0.0f, 0.0f, -2.0f};
        sphere->transform().scale = {0.5f, 0.5f, 0.5f};
        auto sphereMesh = std::make_unique<MeshRenderer>();
        sphereMesh->shape = scene::MeshShape::Sphere;
        sphere->addComponent(std::move(sphereMesh));

        GameObject* plane = scene.createObject("Plane");
        plane->transform().position = {3.0f, 0.0f, -2.0f};
        plane->transform().scale = {0.5f, 0.5f, 0.5f};
        auto planeMesh = std::make_unique<MeshRenderer>();
        planeMesh->shape = scene::MeshShape::Plane;
        plane->addComponent(std::move(planeMesh));

        Vec3 origin = {0.0f, 0.0f, 0.0f};
        Vec3 dir = {0.0f, 0.0f, -1.0f};
        assert(scene.pickObject(origin, dir) == sphere);

        Vec3 planeOrigin = {3.0f, 1.0f, -2.0f};
        Vec3 planeDir = {0.0f, -1.0f, 0.0f};
        assert(scene.pickObject(planeOrigin, planeDir) == plane);
    }
}
