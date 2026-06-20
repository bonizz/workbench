#include "debug/debug_state.h"

#include "scene/scene.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
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

constexpr float kRadToDeg = 180.0f / 3.14159265f;

std::string formatVec3(const Vec3& v)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(2);
    out << v.x << ", " << v.y << ", " << v.z;
    return out.str();
}

Vec3 toDegrees(const Vec3& radians)
{
    return {radians.x * kRadToDeg, radians.y * kRadToDeg, radians.z * kRadToDeg};
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

} // namespace

std::string build(uint64_t frame,
                  float fps,
                  float frameTimeMs,
                  size_t renderCommandCount,
                  const Scene& scene,
                  const GameObject* selected,
                  const std::string& lastScriptPath,
                  const std::string& lastCapturePath,
                  const std::string& lastBundlePath,
                  const std::string* lastAssertionFailure)
{
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
        out << "  position: " << formatVec3(obj->transform().position) << "\n";
        out << "  rotation: " << formatVec3(toDegrees(obj->transform().rotation)) << "\n";
        out << "  scale:    " << formatVec3(obj->transform().scale) << "\n";
        out << "  components: ";
        if (obj->components().empty()) {
            out << "none";
        } else {
            bool first = true;
            for (const auto& comp : obj->components()) {
                if (!first) out << ", ";
                first = false;
                out << comp->typeName();
            }
        }
        out << "\n\n";
    }

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
