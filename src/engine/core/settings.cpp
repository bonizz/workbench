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
