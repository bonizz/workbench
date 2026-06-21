#include "core/settings.h"

#include "json/json.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace Settings {

namespace {

using json = nlohmann::json;

std::string gSettingsPath = "settings.json";

json loadJson()
{
    json j = json::object();
    if (!std::filesystem::exists(gSettingsPath)) {
        return j;
    }

    std::ifstream file(gSettingsPath);
    if (!file) {
        return j;
    }

    try {
        file >> j;
    } catch (...) {
        // Corrupt or empty file: start fresh.
        j = json::object();
    }

    return j;
}

void saveJson(const json& j)
{
    std::ofstream file(gSettingsPath);
    if (!file) {
        return;
    }

    file << j.dump(4) << "\n";
}

} // namespace

bool loadWindowSize(int& width, int& height)
{
    json j = loadJson();
    json window = j.value("window", json::object());

    if (!window.contains("width") || !window.contains("height")) {
        return false;
    }

    try {
        width = window["width"].get<int>();
        height = window["height"].get<int>();
    } catch (...) {
        return false;
    }

    return true;
}

void saveWindowSize(int width, int height)
{
    json j = loadJson();
    j["window"]["width"] = width;
    j["window"]["height"] = height;
    saveJson(j);
}

bool loadWindowPosition(int& x, int& y)
{
    json j = loadJson();
    json window = j.value("window", json::object());

    if (!window.contains("x") || !window.contains("y")) {
        return false;
    }

    try {
        x = window["x"].get<int>();
        y = window["y"].get<int>();
    } catch (...) {
        return false;
    }

    return true;
}

void saveWindowPosition(int x, int y)
{
    json j = loadJson();
    j["window"]["x"] = x;
    j["window"]["y"] = y;
    saveJson(j);
}

bool loadEditorWindowStates(std::unordered_map<std::string, bool>& states)
{
    json j = loadJson();
    json editor = j.value("editor", json::object());

    bool foundAny = false;
    for (auto it = editor.begin(); it != editor.end(); ++it) {
        const std::string& key = it.key();
        if (key.starts_with("show_") && it.value().is_boolean()) {
            states[key.substr(5)] = it.value().get<bool>();
            foundAny = true;
        }
    }

    return foundAny;
}

void saveEditorWindowStates(const std::unordered_map<std::string, bool>& states)
{
    json j = loadJson();
    json editor = j.value("editor", json::object());

    // Remove stale show_ entries before rewriting current visibility.
    for (auto it = editor.begin(); it != editor.end();) {
        if (it.key().starts_with("show_")) {
            it = editor.erase(it);
        } else {
            ++it;
        }
    }

    for (const auto& [name, visible] : states) {
        editor["show_" + name] = visible;
    }

    j["editor"] = editor;
    saveJson(j);
}

bool loadCamera(Vec3& position, Vec3& rotation, float& moveSpeed)
{
    json j = loadJson();
    if (!j.contains("camera")) {
        return false;
    }

    json camera = j["camera"];
    try {
        position.x = camera["position"][0].get<float>();
        position.y = camera["position"][1].get<float>();
        position.z = camera["position"][2].get<float>();

        rotation.x = camera["rotation"][0].get<float>();
        rotation.y = camera["rotation"][1].get<float>();
        rotation.z = camera["rotation"][2].get<float>();

        moveSpeed = camera["moveSpeed"].get<float>();
    } catch (...) {
        return false;
    }

    return true;
}

void saveCamera(const Vec3& position, const Vec3& rotation, float moveSpeed)
{
    json j = loadJson();
    json camera = json::object();
    camera["position"] = {position.x, position.y, position.z};
    camera["rotation"] = {rotation.x, rotation.y, rotation.z};
    camera["moveSpeed"] = moveSpeed;
    j["camera"] = camera;
    saveJson(j);
}

bool loadLighting(LightSettings& light, SkySettings& sky)
{
    json j = loadJson();
    if (!j.contains("lighting")) {
        return false;
    }

    json l = j["lighting"];
    try {
        if (l.contains("light")) {
            json lt = l["light"];
            light.direction = {lt["direction"][0].get<float>(),
                               lt["direction"][1].get<float>(),
                               lt["direction"][2].get<float>()};
            light.ambient = lt["ambient"].get<float>();
            light.diffuse = lt["diffuse"].get<float>();
        }
        if (l.contains("sky")) {
            json sk = l["sky"];
            sky.horizonColor = {sk["horizonColor"][0].get<float>(),
                                sk["horizonColor"][1].get<float>(),
                                sk["horizonColor"][2].get<float>()};
            sky.zenithColor = {sk["zenithColor"][0].get<float>(),
                               sk["zenithColor"][1].get<float>(),
                               sk["zenithColor"][2].get<float>()};
            sky.sunColor = {sk["sunColor"][0].get<float>(),
                            sk["sunColor"][1].get<float>(),
                            sk["sunColor"][2].get<float>()};
            sky.sunSize = sk["sunSize"].get<float>();
            sky.sunIntensity = sk["sunIntensity"].get<float>();
        }
    } catch (...) {
        return false;
    }

    return true;
}

void saveLighting(const LightSettings& light, const SkySettings& sky)
{
    json j = loadJson();

    json l = json::object();
    l["light"] = {
        {"direction", {light.direction.x, light.direction.y, light.direction.z}},
        {"ambient", light.ambient},
        {"diffuse", light.diffuse},
    };
    l["sky"] = {
        {"horizonColor", {sky.horizonColor.x, sky.horizonColor.y, sky.horizonColor.z}},
        {"zenithColor", {sky.zenithColor.x, sky.zenithColor.y, sky.zenithColor.z}},
        {"sunColor", {sky.sunColor.x, sky.sunColor.y, sky.sunColor.z}},
        {"sunSize", sky.sunSize},
        {"sunIntensity", sky.sunIntensity},
    };

    j["lighting"] = l;
    saveJson(j);
}

void setSettingsPath(const std::string& path)
{
    gSettingsPath = path;
}

std::string settingsDirectory()
{
    std::filesystem::path p(gSettingsPath);
    std::filesystem::path dir = p.parent_path();
    return dir.empty() ? std::string(".") : dir.string();
}

} // namespace Settings
