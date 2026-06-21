#include "core/settings.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

namespace Settings {

namespace {

std::string gSettingsPath = "settings.txt";

std::string trim(std::string s)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool parseBool(const std::string& value)
{
    std::string lower = toLower(trim(value));
    return lower == "true" || lower == "1";
}

std::string boolToString(bool value)
{
    return value ? "true" : "false";
}

std::map<std::string, std::string> loadAll()
{
    std::map<std::string, std::string> values;
    if (!std::filesystem::exists(gSettingsPath)) {
        return values;
    }

    std::ifstream file(gSettingsPath);
    if (!file) {
        return values;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }

        auto equals = trimmed.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        std::string key = trim(trimmed.substr(0, equals));
        std::string value = trim(trimmed.substr(equals + 1));
        if (!key.empty()) {
            values[key] = value;
        }
    }

    return values;
}

void saveAll(const std::map<std::string, std::string>& values)
{
    std::ofstream file(gSettingsPath);
    if (!file) {
        return;
    }

    file << "# Workbench settings\n";
    for (const auto& [key, value] : values) {
        file << key << "=" << value << "\n";
    }
}

} // namespace

bool loadWindowSize(int& width, int& height)
{
    auto values = loadAll();

    bool gotWidth = false;
    bool gotHeight = false;

    try {
        auto it = values.find("window_width");
        if (it != values.end()) {
            width = std::stoi(it->second);
            gotWidth = true;
        }

        it = values.find("window_height");
        if (it != values.end()) {
            height = std::stoi(it->second);
            gotHeight = true;
        }
    } catch (...) {
        // Ignore malformed values.
    }

    return gotWidth && gotHeight;
}

void saveWindowSize(int width, int height)
{
    auto values = loadAll();
    values["window_width"] = std::to_string(width);
    values["window_height"] = std::to_string(height);
    saveAll(values);
}

bool loadEditorWindowStates(std::unordered_map<std::string, bool>& states)
{
    auto values = loadAll();

    constexpr const char* kPrefix = "show_";
    constexpr size_t kPrefixLen = 5;

    bool foundAny = false;
    for (const auto& [key, value] : values) {
        if (key.compare(0, kPrefixLen, kPrefix) == 0) {
            std::string name = key.substr(kPrefixLen);
            if (!name.empty()) {
                states[name] = parseBool(value);
                foundAny = true;
            }
        }
    }

    return foundAny;
}

void saveEditorWindowStates(const std::unordered_map<std::string, bool>& states)
{
    auto values = loadAll();

    // Remove any stale show_ entries so panels that no longer exist are cleaned
    // out and current visibility is rewritten below.
    for (auto it = values.begin(); it != values.end();) {
        if (it->first.compare(0, 5, "show_") == 0) {
            it = values.erase(it);
        } else {
            ++it;
        }
    }

    for (const auto& [name, visible] : states) {
        values["show_" + name] = boolToString(visible);
    }

    saveAll(values);
}

void setSettingsPath(const std::string& path)
{
    gSettingsPath = path;
}

} // namespace Settings
