#include "agent/command.h"

#include "agent/script_runner.h"
#include "core/math.h"
#include "debug/debug_state.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
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

constexpr float kRadToDeg = 180.0f / 3.14159265f;

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

AgentCommandResult makeError(const std::string& message)
{
    return AgentCommandResult{false, message};
}

AgentCommandResult makeSuccess(const std::string& message)
{
    return AgentCommandResult{true, message};
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
        ctx.lastScriptPath);
    return makeSuccess(text);
}

namespace {

std::filesystem::path scriptPath(const std::string& filename)
{
    std::filesystem::path dir = "assets/scripts";
    std::filesystem::create_directories(dir);
    return dir / filename;
}

} // namespace

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
        {{"script.run",
          "script.run <filename>",
          "Runs an agent command script from assets/scripts/<filename>.",
          "script.run create_test_scene.wbs"},
         cmdScriptRun},
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
