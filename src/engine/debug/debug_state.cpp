#include "debug/debug_state.h"

#include "scene/scene.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
#include "scene/rotate_component.h"
#include "scene/transform.h"
#include "core/math.h"

using scene::MeshRenderer;

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace DebugState {

namespace {

std::string formatVec3(const Vec3& v)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(2);
    out << v.x << ", " << v.y << ", " << v.z;
    return out.str();
}

Vec3 worldPositionOf(const GameObject* obj)
{
    const Mat4& m = obj->transform().worldMatrix();
    return {m.columns[3][0], m.columns[3][1], m.columns[3][2]};
}

std::vector<const GameObject*> sortedObjects(const Scene& scene)
{
    std::vector<const GameObject*> result;
    for (const auto& obj : scene.objects()) {
        result.push_back(obj.get());
    }
    std::sort(result.begin(), result.end(),
              [](const GameObject* a, const GameObject* b) {
                  return a->id().value < b->id().value;
              });
    return result;
}

void writeHierarchyNode(std::ostringstream& out, const GameObject* obj, int depth)
{
    for (int i = 0; i <= depth; ++i) out << "  ";
    out << "[" << obj->id().value << "] " << obj->name() << "\n";
    for (const GameObject* child : obj->children()) {
        writeHierarchyNode(out, child, depth + 1);
    }
}

void writeHierarchy(std::ostringstream& out, const Scene& scene)
{
    out << "Hierarchy:\n";
    for (const auto& root : scene.objects()) {
        if (root->parent() == nullptr) {
            writeHierarchyNode(out, root.get(), 0);
        }
    }
}

} // namespace

std::string build(uint64_t frame,
                  float fps,
                  float frameTimeMs,
                  size_t renderCommandCount,
                  Scene& scene,
                  const GameObject* selected,
                  const std::string& lastScriptPath,
                  const std::string& lastCapturePath,
                  const std::string& lastBundlePath,
                  const std::string* lastAssertionFailure)
{
    // Refresh derived world matrices first so worldPosition values are accurate.
    scene.updateWorldTransforms();

    std::ostringstream out;
    out << std::fixed << std::setprecision(1);

    out << "Workbench State\n\n";
    out << "Frame: " << frame << "\n";
    out << "FPS: " << fps << "\n";
    out << "FrameTime: " << frameTimeMs << " ms\n";
    if (!scene.loadedScenePath().empty()) {
        out << "SceneFile: " << scene.loadedScenePath() << "\n";
    }
    if (!lastScriptPath.empty()) {
        out << "Last Script: " << lastScriptPath << "\n";
    }
    if (!lastCapturePath.empty()) {
        out << "Last Capture: " << lastCapturePath << "\n";
    }
    if (!lastBundlePath.empty()) {
        out << "Last Bundle: " << lastBundlePath << "\n";
    }
    if (lastAssertionFailure && !lastAssertionFailure->empty()) {
        out << "Last Assertion Failure: " << *lastAssertionFailure << "\n";
    }
    out << "\n";

    out << "Scene:\n";
    for (const GameObject* obj : sortedObjects(scene)) {
        out << "[" << obj->id().value << "] " << obj->name() << "\n";
        if (const GameObject* p = obj->parent()) {
            out << "  parent: " << p->name() << " [" << p->id().value << "]\n";
        } else {
            out << "  parent: none\n";
        }
        out << "  position: " << formatVec3(obj->transform().position) << "\n";
        out << "  rotation: " << formatVec3(toDegrees(obj->transform().rotation)) << "\n";
        out << "  scale:    " << formatVec3(obj->transform().scale) << "\n";
        out << "  worldPosition: " << formatVec3(worldPositionOf(obj)) << "\n";
        out << "  components: ";
        if (obj->components().empty()) {
            out << "none";
        } else {
            bool first = true;
            for (const auto& comp : obj->components()) {
                if (!first) out << ", ";
                first = false;
                out << comp->typeName();
                if (auto* mr = dynamic_cast<const MeshRenderer*>(comp.get())) {
                    out << " (mesh: " << scene::meshShapeToString(mr->shape) << ")";
                } else if (auto* rot = dynamic_cast<const scene::RotateComponent*>(comp.get())) {
                    out << " (angularVelocity: " << formatVec3(rot->angularVelocityEuler) << ")";
                }
            }
        }
        out << "\n\n";
    }

    writeHierarchy(out, scene);
    out << "\n";

    out << "Editor:\n";
    if (selected) {
        out << "  selected: " << selected->name() << " [" << selected->id().value << "]\n";
    } else {
        out << "  selected: none\n";
    }
    out << "\n";

    out << "Renderer:\n";
    out << "  renderCommands: " << renderCommandCount << "\n";

    return out.str();
}

void writeToFile(const std::string& text, const std::string& path)
{
    std::filesystem::path filePath(path);
    std::filesystem::create_directories(filePath.parent_path());

    std::ofstream out(filePath);
    if (out.is_open()) {
        out << text;
    }
}

} // namespace DebugState
