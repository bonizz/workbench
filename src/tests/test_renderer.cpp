#include "core/math.h"
#include "renderer/mesh_geometry.h"
#include "renderer/render_command.h"
#include "renderer/render_context.h"
#include "scene/camera.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
#include "scene/scene.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>

using scene::MeshRenderer;

void runTestRenderer()
{
    // RenderContext command accumulation.
    {
        RenderContext ctx;
        ctx.setCamera(identity(), identity());
        ctx.drawShape(ShapeType::Cube, identity(), {1.0f, 0.0f, 0.0f, 1.0f});
        ctx.drawShape(ShapeType::Cube, identity(), {0.0f, 1.0f, 0.0f, 1.0f});
        ctx.drawGround({0.0f, 0.0f, 0.0f});

        assert(ctx.commands().size() == 3);
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

    auto countHighlights = [](const RenderContext& ctx) {
        std::size_t n = 0;
        for (const auto& cmd : ctx.commands()) {
            if (cmd.highlight) ++n;
        }
        return n;
    };

    // Selecting a MeshRenderer object emits exactly one highlight overlay,
    // matching the object's shape, drawn right after its normal command, then a
    // translate gizmo handle (opaque sphere) at the object's origin.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* sphereObj = scene.createObject("Sphere");
        sphereObj->transform().position = {4.0f, 1.0f, -2.0f};
        auto sphereMesh = std::make_unique<MeshRenderer>();
        sphereMesh->shape = scene::MeshShape::Sphere;
        sphereObj->addComponent(std::move(sphereMesh));

        RenderContext ctx;
        scene.buildRenderCommands(ctx, sphereObj);

        const auto& cmds = ctx.commands();
        assert(cmds.size() == 4); // ground + sphere + highlight + gizmo handle
        assert(cmds[1].type == ShapeType::Sphere);
        assert(!cmds[1].highlight);
        assert(cmds[2].type == ShapeType::Sphere);
        assert(cmds[2].highlight);
        assert(countHighlights(ctx) == 1);

        // The gizmo handle: opaque yellow sphere, not a highlight, sitting at
        // the selected object's world origin.
        const RenderCommand& gizmo = cmds[3];
        assert(gizmo.type == ShapeType::Sphere);
        assert(!gizmo.highlight);
        assert(gizmo.color.w == 1.0f); // opaque, unlike the translucent highlight
        simd::float4 gp = gizmo.transform.columns[3];
        assert(std::fabs(gp.x - 4.0f) < 1e-4f);
        assert(std::fabs(gp.y - 1.0f) < 1e-4f);
        assert(std::fabs(gp.z + 2.0f) < 1e-4f);
    }

    // No highlight for a null selection.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* cubeObj = scene.createObject("Cube");
        cubeObj->addComponent(std::make_unique<MeshRenderer>());

        RenderContext ctx;
        scene.buildRenderCommands(ctx, nullptr);

        assert(ctx.commands().size() == 2); // ground + cube, no overlay
        assert(countHighlights(ctx) == 0);
    }

    // The camera is never highlightable (it has no MeshRenderer).
    {
        Scene scene;
        Camera* cam = scene.createCamera({0.0f, 0.0f, 0.0f});

        RenderContext ctx;
        scene.buildRenderCommands(ctx, cam);

        assert(countHighlights(ctx) == 0);
    }

    // An inactive object is neither drawn nor highlighted, even when selected.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* cubeObj = scene.createObject("Cube");
        cubeObj->addComponent(std::make_unique<MeshRenderer>());
        cubeObj->setActive(false);

        RenderContext ctx;
        scene.buildRenderCommands(ctx, cubeObj);

        assert(ctx.commands().size() == 1); // ground only
        assert(countHighlights(ctx) == 0);
    }

    // An object without a MeshRenderer is not highlighted.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* empty = scene.createObject("Empty");

        RenderContext ctx;
        scene.buildRenderCommands(ctx, empty);

        assert(ctx.commands().size() == 1); // ground only
        assert(countHighlights(ctx) == 0);
    }

    // The highlight respects world transforms: a selected child under a
    // translated parent gets an overlay centered on the child's world position.
    {
        Scene scene;
        scene.createCamera({0.0f, 0.0f, 0.0f});
        GameObject* parent = scene.createObject("Parent");
        parent->transform().position = {5.0f, 0.0f, 0.0f};
        GameObject* child = scene.createObject("Child");
        child->transform().position = {2.0f, 0.0f, 0.0f};
        child->addComponent(std::make_unique<MeshRenderer>());
        scene.setParent(child, parent);

        RenderContext ctx;
        scene.buildRenderCommands(ctx, child);

        assert(countHighlights(ctx) == 1);
        const RenderCommand* highlight = nullptr;
        for (const auto& cmd : ctx.commands()) {
            if (cmd.highlight) highlight = &cmd;
        }
        assert(highlight != nullptr);
        // Scaling about the local origin leaves the translation column intact,
        // so the overlay sits at the child's world position (5 + 2 = 7).
        simd::float4 t = highlight->transform.columns[3];
        assert(std::fabs(t.x - 7.0f) < 1e-4f);
        assert(std::fabs(t.y - 0.0f) < 1e-4f);
        assert(std::fabs(t.z - 0.0f) < 1e-4f);
    }
}
