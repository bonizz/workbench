#include "scene/scene_serializer.h"

#include "json/json.hpp"
#include "scene/camera.h"
#include "scene/game_object.h"
#include "scene/mesh_renderer.h"
#include "scene/rotate_component.h"
#include "scene/scene.h"
#include "scene/scene_document.h"
#include "scene/transform.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

using scene::Component;
using scene::MeshRenderer;
using json = nlohmann::json;

namespace SceneSerializer {

namespace {

json vec3ToJson(const Vec3& v)
{
    return {v.x, v.y, v.z};
}

json float3ToJson(const simd::float3& v)
{
    return {v.x, v.y, v.z};
}

json float4ToJson(const simd::float4& v)
{
    return {v.x, v.y, v.z, v.w};
}

bool isNumberArray(const json& j, size_t expectedSize)
{
    return j.is_array() && j.size() == expectedSize &&
           std::all_of(j.begin(), j.end(), [](const json& e) { return e.is_number(); });
}

Vec3 jsonToVec3(const json& j, const Vec3& fallback)
{
    if (!isNumberArray(j, 3)) {
        return fallback;
    }
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

simd::float3 jsonToFloat3(const json& j, const simd::float3& fallback)
{
    if (!isNumberArray(j, 3)) {
        return fallback;
    }
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

simd::float4 jsonToFloat4(const json& j, const simd::float4& fallback)
{
    if (!isNumberArray(j, 4)) {
        return fallback;
    }
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()};
}

float jsonToFloat(const json& j, float fallback)
{
    if (!j.is_number()) {
        return fallback;
    }
    return j.get<float>();
}

void addWarning(std::string* warning, const std::string& message)
{
    if (!warning) {
        return;
    }
    if (!warning->empty()) {
        *warning += "\n";
    }
    *warning += message;
}

json writeObject(const GameObject& obj)
{
    const Transform& t = obj.transform();
    json o;
    o["name"] = obj.name();
    o["position"] = vec3ToJson(t.position);
    o["rotation"] = vec3ToJson(t.rotation);
    o["scale"] = vec3ToJson(t.scale);

    json comps = json::array();
    for (const auto& comp : obj.components()) {
        if (auto* mesh = dynamic_cast<const MeshRenderer*>(comp.get())) {
            json c;
            c["type"] = "MeshRenderer";
            c["mesh"] = scene::meshShapeToString(mesh->shape);
            c["color"] = float4ToJson(mesh->color);
            comps.push_back(std::move(c));
        } else if (auto* rot = dynamic_cast<const scene::RotateComponent*>(comp.get())) {
            json c;
            c["type"] = "RotateComponent";
            c["angularVelocity"] = vec3ToJson(rot->angularVelocityEuler);
            comps.push_back(std::move(c));
        }
    }
    o["components"] = std::move(comps);

    if (!obj.children().empty()) {
        json children = json::array();
        for (const GameObject* child : obj.children()) {
            children.push_back(writeObject(*child));
        }
        o["children"] = std::move(children);
    }

    return o;
}

bool parseComponent(GameObject* obj, const json& j, std::string& error)
{
    if (!j.is_object()) {
        error = "Expected component object";
        return false;
    }

    std::string type = j.value("type", "");
    if (type == "MeshRenderer") {
        scene::MeshShape shape = scene::MeshShape::Cube;
        std::string mesh = j.value("mesh", "cube");
        if (!scene::meshShapeFromString(mesh, shape)) {
            error = "Unknown mesh shape: " + mesh;
            return false;
        }
        auto meshRenderer = std::make_unique<MeshRenderer>();
        meshRenderer->shape = shape;
        meshRenderer->color = jsonToFloat4(j.value("color", json::array()), meshRenderer->color);
        obj->addComponent(std::move(meshRenderer));
    } else if (type == "RotateComponent") {
        auto rotator = std::make_unique<scene::RotateComponent>();
        rotator->angularVelocityEuler = jsonToVec3(j.value("angularVelocity", json::array()), rotator->angularVelocityEuler);
        obj->addComponent(std::move(rotator));
    } else {
        error = "Unknown component type: " + type;
        return false;
    }

    return true;
}

bool parseObject(Scene& scene, const json& j, std::string& error, GameObject* parent, std::string* warning)
{
    if (!j.is_object()) {
        error = "Expected object entry";
        return false;
    }

    GameObject* obj = scene.createObject(j.value("name", "Object"));
    obj->transform().position = jsonToVec3(j.value("position", json::array()), obj->transform().position);
    obj->transform().rotation = jsonToVec3(j.value("rotation", json::array()), obj->transform().rotation);
    obj->transform().scale = jsonToVec3(j.value("scale", json::array()), obj->transform().scale);

    const json& components = j.value("components", json::array());
    for (const auto& comp : components) {
        if (!parseComponent(obj, comp, error)) {
            return false;
        }
    }

    const json& children = j.value("children", json::array());
    for (const auto& child : children) {
        if (!parseObject(scene, child, error, obj, warning)) {
            return false;
        }
    }

    if (parent && !scene.setParent(obj, parent)) {
        error = "Failed to attach child '" + obj->name() + "' to parent";
        return false;
    }

    if (!obj->hasComponent<MeshRenderer>()) {
        addWarning(warning, "Object '" + obj->name() + "' has no MeshRenderer and will not render.");
    }

    return true;
}

void loadCamera(Camera& camera, const json& j)
{
    camera.transform().position = jsonToVec3(j.value("position", json::array()), camera.transform().position);
    camera.transform().rotation = jsonToVec3(j.value("rotation", json::array()), camera.transform().rotation);
    camera.setMoveSpeed(jsonToFloat(j.value("moveSpeed", camera.moveSpeed()), camera.moveSpeed()));
}

void loadEnvironment(SceneEnvironment& env, const json& j)
{
    const json& light = j.value("light", json::object());
    env.light.direction = jsonToFloat3(light.value("direction", json::array()), env.light.direction);
    env.light.ambient = jsonToFloat(light.value("ambient", env.light.ambient), env.light.ambient);
    env.light.diffuse = jsonToFloat(light.value("diffuse", env.light.diffuse), env.light.diffuse);

    const json& sky = j.value("sky", json::object());
    env.sky.horizonColor = jsonToFloat3(sky.value("horizonColor", json::array()), env.sky.horizonColor);
    env.sky.zenithColor = jsonToFloat3(sky.value("zenithColor", json::array()), env.sky.zenithColor);
    env.sky.sunColor = jsonToFloat3(sky.value("sunColor", json::array()), env.sky.sunColor);
    env.sky.sunSize = jsonToFloat(sky.value("sunSize", env.sky.sunSize), env.sky.sunSize);
    env.sky.sunIntensity = jsonToFloat(sky.value("sunIntensity", env.sky.sunIntensity), env.sky.sunIntensity);
}

// Builds the full scene document (camera + environment + objects tree).
// Shared by save() (to disk, pretty) and serialize() (to string, compact).
json buildDocument(const Scene& scene)
{
    json j;
    j["version"] = kSceneVersion;

    const Camera& cam = scene.camera();
    json camera;
    camera["position"] = vec3ToJson(cam.transform().position);
    camera["rotation"] = vec3ToJson(cam.transform().rotation);
    camera["moveSpeed"] = cam.moveSpeed();
    j["camera"] = std::move(camera);

    const SceneEnvironment& env = scene.environment();
    json environment;
    environment["light"] = {
        {"direction", float3ToJson(env.light.direction)},
        {"ambient", env.light.ambient},
        {"diffuse", env.light.diffuse},
    };
    environment["sky"] = {
        {"horizonColor", float3ToJson(env.sky.horizonColor)},
        {"zenithColor", float3ToJson(env.sky.zenithColor)},
        {"sunColor", float3ToJson(env.sky.sunColor)},
        {"sunSize", env.sky.sunSize},
        {"sunIntensity", env.sky.sunIntensity},
    };
    j["environment"] = std::move(environment);

    j["settings"] = json::object();

    json objects = json::array();
    for (const auto& obj : scene.objects()) {
        if (scene.isCamera(obj.get())) {
            continue;
        }
        if (obj->parent() != nullptr) {
            // Children are serialized nested under their parent; skip them here.
            continue;
        }
        objects.push_back(writeObject(*obj));
    }
    j["objects"] = std::move(objects);

    return j;
}

// Applies a parsed document to the scene. When applyCamera is false the camera
// is left untouched (used by deserialize() so undo never moves the camera).
bool applyDocument(Scene& scene, const json& j, std::string& error, std::string* warning, bool applyCamera)
{
    if (!j.is_object()) {
        error = "Scene file must contain a JSON object";
        return false;
    }

    int version = j.value("version", kSceneVersion);
    if (version > kSceneVersion) {
        addWarning(warning, "Unknown scene version: " + std::to_string(version) + "; best-effort load.");
    }

    if (applyCamera) {
        loadCamera(scene.camera(), j.value("camera", json::object()));
    }
    loadEnvironment(scene.environment(), j.value("environment", json::object()));

    scene.clearObjects();

    const json& objects = j.value("objects", json::array());
    for (const auto& obj : objects) {
        if (!parseObject(scene, obj, error, nullptr, warning)) {
            return false;
        }
    }

    return true;
}

} // namespace

bool save(const Scene& scene, const std::string& path, std::string& error)
{
    std::filesystem::path filePath(path);
    try {
        std::filesystem::create_directories(filePath.parent_path());
    } catch (const std::exception& e) {
        error = std::string("Failed to create directory: ") + e.what();
        return false;
    }

    json j = buildDocument(scene);

    std::ofstream out(filePath);
    if (!out.is_open()) {
        error = "Failed to open file for writing: " + path;
        return false;
    }

    out << j.dump(4) << "\n";
    return true;
}

bool load(Scene& scene, const std::string& path, std::string& error, std::string* warning)
{
    std::ifstream in(path);
    if (!in.is_open()) {
        error = "Failed to open file for reading: " + path;
        return false;
    }

    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.fail() && !in.eof()) {
        error = "Failed to read file: " + path;
        return false;
    }

    json j;
    try {
        j = json::parse(text);
    } catch (const std::exception& e) {
        error = std::string("Failed to parse JSON: ") + e.what();
        return false;
    } catch (...) {
        error = "Failed to parse JSON";
        return false;
    }

    return applyDocument(scene, j, error, warning, /*applyCamera=*/true);
}

std::string serialize(const Scene& scene)
{
    return buildDocument(scene).dump();
}

bool deserialize(Scene& scene, const std::string& text, std::string& error)
{
    json j;
    try {
        j = json::parse(text);
    } catch (const std::exception& e) {
        error = std::string("Failed to parse JSON: ") + e.what();
        return false;
    } catch (...) {
        error = "Failed to parse JSON";
        return false;
    }

    return applyDocument(scene, j, error, /*warning=*/nullptr, /*applyCamera=*/false);
}

} // namespace SceneSerializer
