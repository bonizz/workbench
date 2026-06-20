#include "agent/command.h"

#include "core/math.h"
#include "debug/debug_state.h"
#include "scene/game_object.h"
#include "scene/scene.h"
#include "scene/transform.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

namespace {

constexpr float kRadToDeg = 180.0f / 3.14159265f;

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

AgentCommandResult sceneList(const AgentCommandContext& ctx)
{
    std::ostringstream oss;
    oss << "Scene:\n";
    for (const GameObject* obj : sortedObjects(ctx.scene)) {
        oss << "[" << obj->id().value << "] " << obj->name() << "\n";
    }
    return makeSuccess(oss.str());
}

AgentCommandResult sceneSelect(AgentCommandContext& ctx, const std::vector<std::string>& args)
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

AgentCommandResult sceneGetSelected(const AgentCommandContext& ctx)
{
    if (ctx.selected) {
        std::ostringstream oss;
        oss << "Selected: " << ctx.selected->name() << " [" << ctx.selected->id().value << "]";
        return makeSuccess(oss.str());
    }
    return makeSuccess("Selected: none");
}

AgentCommandResult transformGet(const AgentCommandContext& ctx, const std::vector<std::string>& args)
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

AgentCommandResult transformSetPosition(AgentCommandContext& ctx, const std::vector<std::string>& args)
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

AgentCommandResult debugDump(const AgentCommandContext& ctx)
{
    std::string text = DebugState::build(
        ctx.frame, ctx.fps, ctx.frameTimeMs, ctx.renderCommandCount, ctx.scene, ctx.selected);
    return makeSuccess(text);
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

    if (verb == "scene.list") {
        return sceneList(ctx);
    }
    if (verb == "scene.select") {
        return sceneSelect(ctx, args);
    }
    if (verb == "scene.get_selected") {
        return sceneGetSelected(ctx);
    }
    if (verb == "transform.get") {
        return transformGet(ctx, args);
    }
    if (verb == "transform.set_position") {
        return transformSetPosition(ctx, args);
    }
    if (verb == "debug.dump") {
        return debugDump(ctx);
    }

    return makeError("Unknown command: " + verb);
}
