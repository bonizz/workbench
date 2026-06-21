#include "agent/command.h"

#include "agent/script_runner.h"
#include "capture/capture.h"
#include "core/math.h"
#include "debug/bundle.h"
#include "debug/debug_state.h"
#include "renderer/metal_renderer.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
#include "scene/rotate_component.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "scene/transform.h"

using scene::MeshRenderer;

#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct CommandInfo {
    std::string name;
    std::string usage;
    std::string description;
    std::string example;
};

using CommandHandler = std::function<AgentCommandResult(const std::vector<std::string>&,
                                                        AgentCommandContext&)>;

struct CommandEntry {
    CommandInfo info;
    CommandHandler handler;
};

std::string scenePath(const std::string& filename)
{
    return "assets/scenes/" + filename;
}

std::string trim(const std::string& s)
{
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

std::vector<std::string> split(const std::string& s)
{
    std::vector<std::string> tokens;
    std::string current;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                tokens.push_back(std::move(current));
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        tokens.push_back(std::move(current));
    }
    return tokens;
}

bool parseUint64(const std::string& token, uint64_t& out)
{
    const char* begin = token.data();
    const char* end = token.data() + token.size();
    auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parseFloat(const std::string& token, float& out)
{
    try {
        size_t idx = 0;
        out = std::stof(token, &idx);
        return idx == token.size();
    } catch (...) {
        return false;
    }
}

std::string formatVec3(const Vec3& v)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << v.x << ", " << v.y << ", " << v.z;
    return oss.str();
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

GameObject* findObjectByName(Scene& scene, const std::string& name)
{
    for (const auto& obj : scene.objects()) {
        if (obj->name() == name) {
            return obj.get();
        }
    }
    return nullptr;
}

AgentCommandResult makeError(const std::string& message)
{
    return AgentCommandResult{false, message};
}

AgentCommandResult makeSuccess(const std::string& message)
{
    return AgentCommandResult{true, message};
}

AgentCommandResult makeAssertionFailure(AgentCommandContext& ctx, const std::string& message)
{
    if (ctx.lastAssertionFailure) {
        *ctx.lastAssertionFailure = message;
    }
    return AgentCommandResult{false, message};
}

AgentCommandResult assertVec3(AgentCommandContext& ctx,
                              const std::vector<std::string>& args,
                              const std::string& valueKind,
                              const Vec3& actual,
                              const std::string& valueKindParseError = {})
{
    const std::string& parseKind = valueKindParseError.empty() ? valueKind : valueKindParseError;
    float ex = 0.0f, ey = 0.0f, ez = 0.0f;
    if (!parseFloat(args[2], ex) || !parseFloat(args[3], ey) || !parseFloat(args[4], ez)) {
        return makeError("Invalid " + parseKind + " values");
    }

    float tolerance = 0.01f;
    if (args.size() >= 6) {
        if (!parseFloat(args[5], tolerance)) {
            return makeError("Invalid tolerance: " + args[5]);
        }
        if (tolerance < 0.0f) {
            return makeError("Tolerance must be non-negative");
        }
    }

    auto near = [](float a, float b, float tol) {
        return std::fabs(a - b) <= tol;
    };

    if (near(actual.x, ex, tolerance) && near(actual.y, ey, tolerance) && near(actual.z, ez, tolerance)) {
        return makeSuccess("OK");
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    oss << "Expected " << valueKind << ": " << ex << ", " << ey << ", " << ez << "\n";
    oss << "Actual " << valueKind << ":   " << actual.x << ", " << actual.y << ", " << actual.z;
    if (args.size() >= 6) {
        oss << " (tolerance " << tolerance << ")";
    }
    return makeAssertionFailure(ctx, oss.str());
}

AgentCommandResult cmdSceneList(const std::vector<std::string>&,
                                const AgentCommandContext& ctx)
{
    std::ostringstream oss;
    oss << "Scene:\n";
    for (const GameObject* obj : sortedObjects(ctx.scene)) {
        oss << "[" << obj->id().value << "] " << obj->name() << "\n";
    }
    return makeSuccess(oss.str());
}

AgentCommandResult cmdSceneSelect(const std::vector<std::string>& args,
                                  AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: scene.select <id>");
    }
    uint64_t id = 0;
    if (!parseUint64(args[1], id)) {
        return makeError("Invalid object id: " + args[1]);
    }
    GameObject* obj = ctx.scene.findObjectById(id);
    if (!obj) {
        return makeError("Object not found: " + args[1]);
    }
    ctx.selected = obj;
    std::ostringstream oss;
    oss << "Selected: " << obj->name() << " [" << obj->id().value << "]";
    return makeSuccess(oss.str());
}

AgentCommandResult cmdSceneGetSelected(const std::vector<std::string>&,
                                       const AgentCommandContext& ctx)
{
    if (ctx.selected) {
        std::ostringstream oss;
        oss << "Selected: " << ctx.selected->name() << " [" << ctx.selected->id().value << "]";
        return makeSuccess(oss.str());
    }
    return makeSuccess("Selected: none");
}

AgentCommandResult cmdTransformGet(const std::vector<std::string>& args,
                                   const AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: transform.get <id>");
    }
    uint64_t id = 0;
    if (!parseUint64(args[1], id)) {
        return makeError("Invalid object id: " + args[1]);
    }
    GameObject* obj = ctx.scene.findObjectById(id);
    if (!obj) {
        return makeError("Object not found: " + args[1]);
    }
    const Transform& t = obj->transform();
    std::ostringstream oss;
    oss << "Object: " << obj->name() << " [" << obj->id().value << "]\n";
    oss << "Position: " << formatVec3(t.position) << "\n";
    oss << "Rotation: " << formatVec3(toDegrees(t.rotation)) << "\n";
    oss << "Scale:    " << formatVec3(t.scale) << "\n";
    return makeSuccess(oss.str());
}

AgentCommandResult cmdTransformSetPosition(const std::vector<std::string>& args,
                                           AgentCommandContext& ctx)
{
    if (args.size() < 5) {
        return makeError("Usage: transform.set_position <id> <x> <y> <z>");
    }
    uint64_t id = 0;
    if (!parseUint64(args[1], id)) {
        return makeError("Invalid object id: " + args[1]);
    }
    GameObject* obj = ctx.scene.findObjectById(id);
    if (!obj) {
        return makeError("Object not found: " + args[1]);
    }
    float x = 0.0f, y = 0.0f, z = 0.0f;
    if (!parseFloat(args[2], x) || !parseFloat(args[3], y) || !parseFloat(args[4], z)) {
        return makeError("Invalid position values");
    }
    obj->transform().position = {x, y, z};
    std::ostringstream oss;
    oss << "Set position of " << obj->name() << " [" << obj->id().value << "] to "
        << formatVec3(obj->transform().position);
    return makeSuccess(oss.str());
}

AgentCommandResult cmdDebugDump(const std::vector<std::string>&,
                                const AgentCommandContext& ctx)
{
    std::string text = DebugState::build(
        ctx.frame, ctx.fps, ctx.frameTimeMs, ctx.renderCommandCount, ctx.scene, ctx.selected,
        ctx.lastScriptPath, ctx.lastCapturePath, ctx.lastBundlePath);
    return makeSuccess(text);
}

AgentCommandResult cmdDebugBundle(const std::vector<std::string>& args,
                                  AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: debug.bundle <name>");
    }

    const std::string& name = args[1];
    if (!Bundle::isValidBundleName(name)) {
        return makeError("Invalid bundle name: " + name);
    }

    if (!ctx.renderer) {
        return makeError("Renderer not available");
    }

    std::string bundlePath;
    if (!Bundle::createBundle(name, ctx, bundlePath)) {
        return makeError("Failed to create bundle: " + name);
    }

    ctx.lastBundlePath = bundlePath;

    std::ostringstream oss;
    oss << "Created bundle:\n";
    oss << bundlePath << "\n\n";
    oss << "Files:\n";
    oss << "  state.txt      (written synchronously)\n";
    oss << "  screenshot.png (queued asynchronously)\n";
    return makeSuccess(oss.str());
}

namespace {

std::filesystem::path scriptPath(const std::string& filename)
{
    std::filesystem::path dir = "assets/scripts";
    std::filesystem::create_directories(dir);
    return dir / filename;
}

} // namespace

AgentCommandResult cmdRenderCapture(const std::vector<std::string>& args,
                                    AgentCommandContext& ctx)
{
    std::string filename;
    if (args.size() >= 2) {
        filename = args[1];
    }

    if (!filename.empty() && !Capture::isValidFilename(filename)) {
        return makeError("Invalid capture filename: " + filename);
    }

    std::string path = Capture::makeCapturePath(filename);

    if (!ctx.renderer) {
        return makeError("Renderer not available");
    }

    ctx.renderer->requestScreenshot(path);
    ctx.lastCapturePath = path;
    return makeSuccess("Queued screenshot: " + path);
}

AgentCommandResult cmdScriptRun(const std::vector<std::string>& args,
                                AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: script.run <filename>");
    }

    const std::string& filename = args[1];
    std::filesystem::path path = scriptPath(filename);
    if (!std::filesystem::exists(path)) {
        return makeError("Script not found: " + path.string());
    }

    ScriptResult result = runScript(ctx, path, filename);
    if (!result.success) {
        return makeError(result.error);
    }

    std::ostringstream oss;
    oss << "Executed " << result.executed << " command";
    if (result.executed != 1) {
        oss << "s";
    }
    oss << ".\n\n" << result.output;
    return makeSuccess(oss.str());
}

AgentCommandResult cmdSceneCreateCube(const std::vector<std::string>& args,
                                      AgentCommandContext& ctx)
{
    std::string name = "Cube";
    if (args.size() >= 2) {
        name = args[1];
    }

    GameObject* obj = ctx.scene.createObject(name);
    obj->transform().position = {0.0f, 0.5f, 0.0f};
    obj->transform().scale = {0.5f, 0.5f, 0.5f};
    auto mesh = std::make_unique<MeshRenderer>();
    mesh->color = {0.95f, 0.55f, 0.20f, 1.0f};
    obj->addComponent(std::move(mesh));
    ctx.selected = obj;

    std::ostringstream oss;
    oss << "Created " << obj->name() << " [" << obj->id().value << "]";
    return makeSuccess(oss.str());
}

AgentCommandResult cmdSceneDelete(const std::vector<std::string>& args,
                                  AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: scene.delete <id>");
    }
    uint64_t id = 0;
    if (!parseUint64(args[1], id)) {
        return makeError("Invalid object id: " + args[1]);
    }
    GameObject* obj = ctx.scene.findObjectById(id);
    if (!obj) {
        return makeError("Object not found: " + args[1]);
    }
    if (ctx.scene.isCamera(obj)) {
        return makeError("Cannot delete the camera");
    }

    if (ctx.scene.deleteObject(obj)) {
        if (ctx.selected == obj) {
            ctx.selected = nullptr;
        }
        return makeSuccess("Deleted object " + args[1]);
    }
    return makeError("Failed to delete object " + args[1]);
}

AgentCommandResult cmdSceneDuplicate(const std::vector<std::string>& args,
                                     AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: scene.duplicate <id>");
    }
    uint64_t id = 0;
    if (!parseUint64(args[1], id)) {
        return makeError("Invalid object id: " + args[1]);
    }
    GameObject* obj = ctx.scene.findObjectById(id);
    if (!obj) {
        return makeError("Object not found: " + args[1]);
    }
    if (ctx.scene.isCamera(obj)) {
        return makeError("Cannot duplicate the camera");
    }

    GameObject* copy = ctx.scene.duplicateObject(obj);
    if (!copy) {
        return makeError("Failed to duplicate object " + args[1]);
    }

    ctx.selected = copy;
    std::ostringstream oss;
    oss << "Duplicated " << obj->name() << " [" << obj->id().value << "] -> "
        << copy->name() << " [" << copy->id().value << "]";
    return makeSuccess(oss.str());
}

// Hierarchy commands are name-based (stable across load). Camera is excluded
// from both parent and child roles.
AgentCommandResult cmdSceneSetParent(const std::vector<std::string>& args,
                                    AgentCommandContext& ctx)
{
    if (args.size() < 3) {
        return makeError("Usage: scene.set_parent <childName> <parentName>");
    }
    const std::string& childName = args[1];
    const std::string& parentName = args[2];

    GameObject* child = findObjectByName(ctx.scene, childName);
    if (!child) {
        return makeError("Object not found: " + childName);
    }
    GameObject* parent = findObjectByName(ctx.scene, parentName);
    if (!parent) {
        return makeError("Object not found: " + parentName);
    }
    if (ctx.scene.isCamera(child)) {
        return makeError("The camera cannot be a child");
    }
    if (ctx.scene.isCamera(parent)) {
        return makeError("The camera cannot be a parent");
    }
    if (child == parent) {
        return makeError("Cannot parent an object under itself");
    }
    // Only remaining setParent failure is a cycle.
    if (!ctx.scene.setParent(child, parent)) {
        return makeError("Cannot parent '" + childName + "' under '" + parentName
                         + "' (would create a cycle)");
    }

    std::ostringstream oss;
    oss << "Parented " << child->name() << " [" << child->id().value
        << "] under " << parent->name() << " [" << parent->id().value << "]";
    return makeSuccess(oss.str());
}

AgentCommandResult cmdSceneDetach(const std::vector<std::string>& args,
                                  AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: scene.detach <name>");
    }
    const std::string& name = args[1];
    GameObject* obj = findObjectByName(ctx.scene, name);
    if (!obj) {
        return makeError("Object not found: " + name);
    }
    if (ctx.scene.isCamera(obj)) {
        return makeError("The camera cannot be detached");
    }
    if (!obj->parent()) {
        return makeSuccess(obj->name() + " is already a root");
    }
    ctx.scene.setParent(obj, nullptr);
    return makeSuccess("Detached " + obj->name() + " [" + std::to_string(obj->id().value) + "]");
}

AgentCommandResult cmdSceneGetParent(const std::vector<std::string>& args,
                                     AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: scene.get_parent <name>");
    }
    const std::string& name = args[1];
    GameObject* obj = findObjectByName(ctx.scene, name);
    if (!obj) {
        return makeError("Object not found: " + name);
    }
    std::ostringstream oss;
    if (GameObject* p = obj->parent()) {
        oss << "Parent: " << p->name() << " [" << p->id().value << "]";
    } else {
        oss << "Parent: none";
    }
    return makeSuccess(oss.str());
}

AgentCommandResult cmdSceneGetChildren(const std::vector<std::string>& args,
                                       AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: scene.get_children <name>");
    }
    const std::string& name = args[1];
    GameObject* obj = findObjectByName(ctx.scene, name);
    if (!obj) {
        return makeError("Object not found: " + name);
    }
    std::ostringstream oss;
    const auto& kids = obj->children();
    if (kids.empty()) {
        oss << "Children: none";
    } else {
        oss << "Children:\n";
        for (const GameObject* c : kids) {
            oss << c->name() << " [" << c->id().value << "]\n";
        }
    }
    return makeSuccess(oss.str());
}

AgentCommandResult cmdSceneSave(const std::vector<std::string>& args,
                                AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: scene.save <filename>");
    }

    std::string path = scenePath(args[1]);
    std::string error;
    if (!SceneSerializer::save(ctx.scene, path, error)) {
        return makeError(error);
    }

    ctx.scene.setLoadedScenePath(args[1]);
    return makeSuccess("Saved scene to " + path);
}

AgentCommandResult cmdSceneLoad(const std::vector<std::string>& args,
                                AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: scene.load <filename>");
    }

    std::string path = scenePath(args[1]);
    if (!std::filesystem::exists(path)) {
        return makeError("Scene file not found: " + path);
    }

    std::string error;
    std::string warning;
    if (!SceneSerializer::load(ctx.scene, path, error, &warning)) {
        return makeError(error);
    }

    ctx.selected = nullptr;
    ctx.scene.setLoadedScenePath(args[1]);

    std::ostringstream oss;
    oss << "Loaded scene from " << path;
    if (!warning.empty()) {
        oss << "\nWarning:\n" << warning;
    }
    return makeSuccess(oss.str());
}

AgentCommandResult cmdComponentList(const std::vector<std::string>& args,
                                    AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: component.list <id>");
    }
    uint64_t id = 0;
    if (!parseUint64(args[1], id)) {
        return makeError("Invalid object id: " + args[1]);
    }
    GameObject* obj = ctx.scene.findObjectById(id);
    if (!obj) {
        return makeError("Object not found: " + args[1]);
    }

    std::ostringstream oss;
    oss << "Components on " << obj->name() << " [" << id << "]:\n";
    if (obj->components().empty()) {
        oss << "  (none)\n";
    } else {
        for (const auto& comp : obj->components()) {
            oss << "  " << comp->typeName() << "\n";
        }
    }
    return makeSuccess(oss.str());
}

AgentCommandResult cmdComponentAddMeshRenderer(const std::vector<std::string>& args,
                                               AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: component.add_mesh_renderer <id>");
    }
    uint64_t id = 0;
    if (!parseUint64(args[1], id)) {
        return makeError("Invalid object id: " + args[1]);
    }
    GameObject* obj = ctx.scene.findObjectById(id);
    if (!obj) {
        return makeError("Object not found: " + args[1]);
    }
    if (obj->hasComponent<MeshRenderer>()) {
        return makeError(obj->name() + " already has a MeshRenderer");
    }

    auto mesh = std::make_unique<MeshRenderer>();
    mesh->color = {0.95f, 0.55f, 0.20f, 1.0f};
    obj->addComponent(std::move(mesh));
    return makeSuccess("Added MeshRenderer to " + obj->name() + " [" + args[1] + "]");
}

AgentCommandResult cmdComponentAddRotator(const std::vector<std::string>& args,
                                         AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: component.add_rotator <name>");
    }
    const std::string& name = args[1];
    GameObject* obj = findObjectByName(ctx.scene, name);
    if (!obj) {
        return makeError("Object not found: " + name);
    }
    if (obj->hasComponent<scene::RotateComponent>()) {
        return makeError(obj->name() + " already has a RotateComponent");
    }

    obj->addComponent(std::make_unique<scene::RotateComponent>());
    return makeSuccess("Added RotateComponent to " + obj->name());
}

AgentCommandResult cmdComponentSetRotator(const std::vector<std::string>& args,
                                         AgentCommandContext& ctx)
{
    if (args.size() < 5) {
        return makeError("Usage: component.set_rotator <name> <x> <y> <z>");
    }
    const std::string& name = args[1];
    GameObject* obj = findObjectByName(ctx.scene, name);
    if (!obj) {
        return makeError("Object not found: " + name);
    }
    scene::RotateComponent* rot = obj->getComponent<scene::RotateComponent>();
    if (!rot) {
        return makeError(obj->name() + " has no RotateComponent");
    }

    float x = 0.0f, y = 0.0f, z = 0.0f;
    if (!parseFloat(args[2], x) || !parseFloat(args[3], y) || !parseFloat(args[4], z)) {
        return makeError("Invalid angular velocity values");
    }

    rot->angularVelocityEuler = {x, y, z};
    std::ostringstream oss;
    oss << "Set " << obj->name() << " angular velocity to " << formatVec3({x, y, z}) << " deg/s";
    return makeSuccess(oss.str());
}

AgentCommandResult cmdComponentSetMesh(const std::vector<std::string>& args,
                                       AgentCommandContext& ctx)
{
    if (args.size() < 3) {
        return makeError("Usage: component.set_mesh <name> <shape>");
    }
    const std::string& name = args[1];
    GameObject* obj = findObjectByName(ctx.scene, name);
    if (!obj) {
        return makeError("Object not found: " + name);
    }
    MeshRenderer* mesh = obj->getComponent<MeshRenderer>();
    if (!mesh) {
        return makeError(obj->name() + " has no MeshRenderer");
    }

    scene::MeshShape shape;
    if (!scene::meshShapeFromString(args[2], shape)) {
        return makeError("Unknown mesh shape: " + args[2]);
    }

    mesh->shape = shape;
    std::ostringstream oss;
    oss << "Set " << obj->name() << " mesh to " << scene::meshShapeToString(shape);
    return makeSuccess(oss.str());
}

AgentCommandResult cmdAssertMesh(const std::vector<std::string>& args,
                                 AgentCommandContext& ctx)
{
    if (args.size() < 3) {
        return makeError("Usage: assert.mesh <name> <shape>");
    }

    const std::string& name = args[1];
    GameObject* obj = findObjectByName(ctx.scene, name);
    if (!obj) {
        return makeAssertionFailure(ctx, "Object not found: " + name);
    }

    MeshRenderer* mesh = obj->getComponent<MeshRenderer>();
    if (!mesh) {
        return makeAssertionFailure(ctx, "Object '" + name + "' has no MeshRenderer component.");
    }

    scene::MeshShape expected;
    if (!scene::meshShapeFromString(args[2], expected)) {
        return makeError("Unknown mesh shape: " + args[2]);
    }

    if (mesh->shape == expected) {
        return makeSuccess("OK");
    }

    std::ostringstream oss;
    oss << "Expected mesh: " << scene::meshShapeToString(expected) << "\n";
    oss << "Actual mesh:   " << scene::meshShapeToString(mesh->shape);
    return makeAssertionFailure(ctx, oss.str());
}

AgentCommandResult cmdSimStep(const std::vector<std::string>& args,
                              AgentCommandContext& ctx)
{
    float dt = 1.0f / 60.0f;
    if (args.size() >= 2) {
        if (!parseFloat(args[1], dt)) {
            return makeError("Invalid dt: " + args[1]);
        }
        if (dt < 0.0f) {
            return makeError("dt must be non-negative");
        }
    }

    ctx.scene.update(dt);
    std::ostringstream oss;
    oss << "Stepped simulation by " << dt << "s";
    return makeSuccess(oss.str());
}

AgentCommandResult cmdAssertRotation(const std::vector<std::string>& args,
                                     AgentCommandContext& ctx)
{
    if (args.size() < 5) {
        return makeError("Usage: assert.rotation <name> <x> <y> <z> [tolerance]");
    }

    const std::string& name = args[1];
    GameObject* obj = findObjectByName(ctx.scene, name);
    if (!obj) {
        return makeAssertionFailure(ctx, "Object not found: " + name);
    }

    return assertVec3(ctx, args, "rotation", toDegrees(obj->transform().rotation));
}

AgentCommandResult cmdAssertPosition(const std::vector<std::string>& args,
                                     AgentCommandContext& ctx)
{
    if (args.size() < 5) {
        return makeError("Usage: assert.position <name> <x> <y> <z> [tolerance]");
    }

    const std::string& name = args[1];
    GameObject* obj = findObjectByName(ctx.scene, name);
    if (!obj) {
        return makeAssertionFailure(ctx, "Object not found: " + name);
    }

    return assertVec3(ctx, args, "position", obj->transform().position);
}

AgentCommandResult cmdAssertScale(const std::vector<std::string>& args,
                                  AgentCommandContext& ctx)
{
    if (args.size() < 5) {
        return makeError("Usage: assert.scale <name> <x> <y> <z> [tolerance]");
    }

    const std::string& name = args[1];
    GameObject* obj = findObjectByName(ctx.scene, name);
    if (!obj) {
        return makeAssertionFailure(ctx, "Object not found: " + name);
    }

    return assertVec3(ctx, args, "scale", obj->transform().scale);
}

AgentCommandResult cmdAssertColor(const std::vector<std::string>& args,
                                  AgentCommandContext& ctx)
{
    if (args.size() < 6) {
        return makeError("Usage: assert.color <name> <r> <g> <b> <a> [tolerance]");
    }

    const std::string& name = args[1];
    GameObject* obj = findObjectByName(ctx.scene, name);
    if (!obj) {
        return makeAssertionFailure(ctx, "Object not found: " + name);
    }

    MeshRenderer* mesh = obj->getComponent<MeshRenderer>();
    if (!mesh) {
        return makeAssertionFailure(ctx, "Object '" + name + "' has no MeshRenderer component.");
    }

    float er = 0.0f, eg = 0.0f, eb = 0.0f, ea = 0.0f;
    if (!parseFloat(args[2], er) || !parseFloat(args[3], eg) ||
        !parseFloat(args[4], eb) || !parseFloat(args[5], ea)) {
        return makeError("Invalid color values");
    }

    float tolerance = 0.01f;
    if (args.size() >= 7) {
        if (!parseFloat(args[6], tolerance)) {
            return makeError("Invalid tolerance: " + args[6]);
        }
        if (tolerance < 0.0f) {
            return makeError("Tolerance must be non-negative");
        }
    }

    const simd::float4& color = mesh->color;
    auto near = [](float a, float b, float tol) {
        return std::fabs(a - b) <= tol;
    };

    if (near(color.x, er, tolerance) && near(color.y, eg, tolerance) &&
        near(color.z, eb, tolerance) && near(color.w, ea, tolerance)) {
        return makeSuccess("OK");
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    oss << "Expected color: " << er << ", " << eg << ", " << eb << ", " << ea << "\n";
    oss << "Actual color:   " << color.x << ", " << color.y << ", " << color.z << ", " << color.w;
    if (args.size() >= 7) {
        oss << " (tolerance " << tolerance << ")";
    }
    return makeAssertionFailure(ctx, oss.str());
}

AgentCommandResult cmdAssertObjectCount(const std::vector<std::string>& args,
                                        AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: assert.object_count <count>");
    }

    uint64_t expected = 0;
    if (!parseUint64(args[1], expected)) {
        return makeError("Invalid count: " + args[1]);
    }

    size_t actual = ctx.scene.objects().size();
    if (actual == expected) {
        return makeSuccess("OK");
    }

    std::ostringstream oss;
    oss << "Expected object count: " << expected << "\n";
    oss << "Actual object count: " << actual;
    return makeAssertionFailure(ctx, oss.str());
}

AgentCommandResult cmdAssertObjectExists(const std::vector<std::string>& args,
                                         AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        return makeError("Usage: assert.object_exists <name>");
    }

    const std::string& name = args[1];
    if (findObjectByName(ctx.scene, name)) {
        return makeSuccess("OK");
    }

    return makeAssertionFailure(ctx, "Object not found: " + name);
}

AgentCommandResult cmdAssertSelected(const std::vector<std::string>& args,
                                     AgentCommandContext& ctx)
{
    if (args.size() < 2) {
        if (ctx.selected) {
            return makeSuccess("OK");
        }
        return makeAssertionFailure(ctx, "No object selected");
    }

    const std::string& name = args[1];
    if (ctx.selected && ctx.selected->name() == name) {
        return makeSuccess("OK");
    }

    std::string actual = ctx.selected ? ctx.selected->name() : "none";
    return makeAssertionFailure(ctx, "Expected selection: " + name + "\nActual selection: " + actual);
}

AgentCommandResult cmdAssertHasComponent(const std::vector<std::string>& args,
                                         AgentCommandContext& ctx)
{
    if (args.size() < 3) {
        return makeError("Usage: assert.has_component <name> <component_type>");
    }

    const std::string& name = args[1];
    const std::string& type = args[2];

    GameObject* obj = findObjectByName(ctx.scene, name);
    if (!obj) {
        return makeAssertionFailure(ctx, "Object not found: " + name);
    }

    for (const auto& comp : obj->components()) {
        if (comp->typeName() == type) {
            return makeSuccess("OK");
        }
    }

    return makeAssertionFailure(ctx, name + " does not have component " + type);
}

AgentCommandResult cmdAssertParent(const std::vector<std::string>& args,
                                   AgentCommandContext& ctx)
{
    if (args.size() < 3) {
        return makeError("Usage: assert.parent <childName> <parentName|none>");
    }
    const std::string& childName = args[1];
    const std::string& expected = args[2];

    GameObject* child = findObjectByName(ctx.scene, childName);
    if (!child) {
        return makeAssertionFailure(ctx, "Object not found: " + childName);
    }

    GameObject* actual = child->parent();
    if (expected == "none") {
        if (actual == nullptr) {
            return makeSuccess("OK");
        }
        return makeAssertionFailure(ctx, "Expected parent: none\nActual parent: "
                                    + actual->name() + " [" + std::to_string(actual->id().value) + "]");
    }

    if (actual == nullptr) {
        return makeAssertionFailure(ctx, "Expected parent: " + expected + "\nActual parent: none");
    }
    if (actual->name() == expected) {
        return makeSuccess("OK");
    }
    return makeAssertionFailure(ctx, "Expected parent: " + expected + "\nActual parent: "
                                + actual->name() + " [" + std::to_string(actual->id().value) + "]");
}

AgentCommandResult cmdAssertWorldPosition(const std::vector<std::string>& args,
                                          AgentCommandContext& ctx)
{
    if (args.size() < 5) {
        return makeError("Usage: assert.world_position <name> <x> <y> <z> [tolerance]");
    }

    const std::string& name = args[1];
    GameObject* obj = findObjectByName(ctx.scene, name);
    if (!obj) {
        return makeAssertionFailure(ctx, "Object not found: " + name);
    }

    // Refresh defensively before reading the world matrix.
    ctx.scene.updateWorldTransforms();
    const Mat4& m = obj->transform().worldMatrix();
    Vec3 actual{m.columns[3][0], m.columns[3][1], m.columns[3][2]};

    return assertVec3(ctx, args, "world position", actual, "position");
}

const std::vector<CommandEntry>& commandTable();

AgentCommandResult cmdAgentCommands(const std::vector<std::string>&,
                                    AgentCommandContext&)
{
    std::ostringstream oss;
    for (const auto& entry : commandTable()) {
        oss << entry.info.name << "\n";
    }
    return makeSuccess(oss.str());
}

AgentCommandResult cmdAgentHelp(const std::vector<std::string>& args,
                                AgentCommandContext&)
{
    if (args.size() < 2) {
        std::ostringstream oss;
        oss << "Workbench Agent Interface\n\n";
        oss << "Use:\n\n";
        oss << "agent.commands\n\n";
        oss << "to list all available commands.\n\n";
        oss << "Use:\n\n";
        oss << "agent.help <command>\n\n";
        oss << "for command-specific help.\n\n";
        oss << "Commands:\n";
        for (const auto& entry : commandTable()) {
            oss << entry.info.name << "\n";
        }
        return makeSuccess(oss.str());
    }

    const std::string& target = args[1];
    for (const auto& entry : commandTable()) {
        if (entry.info.name == target) {
            std::ostringstream oss;
            oss << entry.info.usage << "\n\n";
            oss << entry.info.description << "\n\n";
            oss << "Example:\n";
            oss << entry.info.example << "\n";
            return makeSuccess(oss.str());
        }
    }
    return makeError("No help available for: " + target);
}

const std::vector<CommandEntry>& commandTable()
{
    static const std::vector<CommandEntry> table = {
        {{"agent.commands",
          "agent.commands",
          "Lists all available commands.",
          "agent.commands"},
         cmdAgentCommands},
        {{"agent.help",
          "agent.help <command>",
          "Shows general help or help for a specific command.",
          "agent.help scene.select"},
         cmdAgentHelp},
        {{"scene.list",
          "scene.list",
          "Lists all GameObjects in the scene.",
          "scene.list"},
         cmdSceneList},
        {{"scene.select",
          "scene.select <id>",
          "Selects a GameObject by ObjectId.",
          "scene.select 2"},
         cmdSceneSelect},
        {{"scene.get_selected",
          "scene.get_selected",
          "Returns the currently selected GameObject.",
          "scene.get_selected"},
         cmdSceneGetSelected},
        {{"scene.create_cube",
          "scene.create_cube [name]",
          "Creates a new GameObject at the origin.",
          "scene.create_cube Cube2"},
         cmdSceneCreateCube},
        {{"scene.delete",
          "scene.delete <id>",
          "Deletes a GameObject by ObjectId.",
          "scene.delete 3"},
         cmdSceneDelete},
        {{"scene.duplicate",
          "scene.duplicate <id>",
          "Duplicates a GameObject by ObjectId.",
          "scene.duplicate 2"},
         cmdSceneDuplicate},
        {{"scene.set_parent",
          "scene.set_parent <childName> <parentName>",
          "Parents a named GameObject under another named GameObject. Camera excluded.",
          "scene.set_parent Child Parent"},
         cmdSceneSetParent},
        {{"scene.detach",
          "scene.detach <name>",
          "Makes a named GameObject a root (detaches it from its parent).",
          "scene.detach Child"},
         cmdSceneDetach},
        {{"scene.get_parent",
          "scene.get_parent <name>",
          "Shows the parent of a named GameObject (or none).",
          "scene.get_parent Child"},
         cmdSceneGetParent},
        {{"scene.get_children",
          "scene.get_children <name>",
          "Lists the children of a named GameObject.",
          "scene.get_children Parent"},
         cmdSceneGetChildren},
        {{"scene.save",
          "scene.save <filename>",
          "Saves authored objects to assets/scenes/<filename>.",
          "scene.save test.scene"},
         cmdSceneSave},
        {{"scene.load",
          "scene.load <filename>",
          "Loads authored objects from assets/scenes/<filename>.",
          "scene.load test.scene"},
         cmdSceneLoad},
        {{"component.list",
          "component.list <id>",
          "Lists components attached to a GameObject.",
          "component.list 2"},
         cmdComponentList},
        {{"component.add_mesh_renderer",
          "component.add_mesh_renderer <id>",
          "Adds a MeshRenderer component to a GameObject.",
          "component.add_mesh_renderer 2"},
         cmdComponentAddMeshRenderer},
        {{"component.add_rotator",
          "component.add_rotator <name>",
          "Adds a RotateComponent (angular velocity 0,0,0) to a named GameObject.",
          "component.add_rotator Spinner"},
         cmdComponentAddRotator},
        {{"component.set_rotator",
          "component.set_rotator <name> <x> <y> <z>",
          "Sets a RotateComponent's angular velocity in degrees per second.",
          "component.set_rotator Spinner 0 90 0"},
         cmdComponentSetRotator},
        {{"component.set_mesh",
          "component.set_mesh <name> <shape>",
          "Sets a MeshRenderer's shape to cube, sphere, or plane.",
          "component.set_mesh Bally sphere"},
         cmdComponentSetMesh},
        {{"sim.step",
          "sim.step [dt]",
          "Advances the simulation by dt seconds (default 1/60). Calls Scene::update.",
          "sim.step 1.0"},
         cmdSimStep},
        {{"transform.get",
          "transform.get <id>",
          "Shows the transform of a GameObject.",
          "transform.get 2"},
         cmdTransformGet},
        {{"transform.set_position",
          "transform.set_position <id> <x> <y> <z>",
          "Updates the position of a GameObject.",
          "transform.set_position 2 1.0 0.0 0.0"},
         cmdTransformSetPosition},
        {{"debug.dump",
          "debug.dump",
          "Returns a full debug state snapshot.",
          "debug.dump"},
         cmdDebugDump},
        {{"debug.bundle",
          "debug.bundle <name>",
          "Creates a repro bundle: state.txt and screenshot.png.",
          "debug.bundle cube_overlap"},
         cmdDebugBundle},
        {{"script.run",
          "script.run <filename>",
          "Runs an agent command script from assets/scripts/<filename>.",
          "script.run create_test_scene.wbs"},
         cmdScriptRun},
        {{"render.capture",
          "render.capture [filename]",
          "Queues a screenshot of the current viewport to captures/<filename>.",
          "render.capture test_scene.png"},
         cmdRenderCapture},
        {{"assert.object_count",
          "assert.object_count <count>",
          "Asserts the total number of GameObjects in the scene (camera included).",
          "assert.object_count 3"},
         cmdAssertObjectCount},
        {{"assert.object_exists",
          "assert.object_exists <name>",
          "Asserts that a GameObject with the given name exists.",
          "assert.object_exists Cube"},
         cmdAssertObjectExists},
        {{"assert.selected",
          "assert.selected [name]",
          "Asserts that an object is selected, optionally matching a specific name.",
          "assert.selected Cube"},
         cmdAssertSelected},
        {{"assert.has_component",
          "assert.has_component <name> <component_type>",
          "Asserts that a GameObject has a specific component.",
          "assert.has_component Cube MeshRenderer"},
         cmdAssertHasComponent},
        {{"assert.parent",
          "assert.parent <childName> <parentName|none>",
          "Asserts a named child's parent (by name), or 'none' if it is a root.",
          "assert.parent Child Parent"},
         cmdAssertParent},
        {{"assert.world_position",
          "assert.world_position <name> <x> <y> <z> [tolerance]",
          "Asserts a named GameObject's WORLD-space position (default tolerance 0.01).",
          "assert.world_position Child 1 0 0"},
         cmdAssertWorldPosition},
        {{"assert.rotation",
          "assert.rotation <name> <x> <y> <z> [tolerance]",
          "Asserts a named GameObject's Euler rotation in degrees (default tolerance 0.01).",
          "assert.rotation Spinner 0 90 0"},
         cmdAssertRotation},
        {{"assert.position",
          "assert.position <name> <x> <y> <z> [tolerance]",
          "Asserts a named GameObject's position (default tolerance 0.01).",
          "assert.position Cube 0 0.5 0"},
         cmdAssertPosition},
        {{"assert.scale",
          "assert.scale <name> <x> <y> <z> [tolerance]",
          "Asserts a named GameObject's scale (default tolerance 0.01).",
          "assert.scale Cube 0.5 0.5 0.5"},
         cmdAssertScale},
        {{"assert.color",
          "assert.color <name> <r> <g> <b> <a> [tolerance]",
          "Asserts a named GameObject's MeshRenderer color rgba (default tolerance 0.01).",
          "assert.color Cube 0.95 0.55 0.20 1.0"},
         cmdAssertColor},
        {{"assert.mesh",
          "assert.mesh <name> <shape>",
          "Asserts a named GameObject's MeshRenderer shape is cube, sphere, or plane.",
          "assert.mesh Bally sphere"},
         cmdAssertMesh},
    };
    return table;
}

} // namespace

AgentCommandResult executeCommand(const std::string& command, AgentCommandContext& ctx)
{
    std::string input = trim(command);
    if (input.empty()) {
        return makeError("Empty command");
    }

    std::vector<std::string> args = split(input);
    const std::string& verb = args[0];

    for (const auto& entry : commandTable()) {
        if (entry.info.name == verb) {
            return entry.handler(args, ctx);
        }
    }

    return makeError("Unknown command: " + verb);
}
