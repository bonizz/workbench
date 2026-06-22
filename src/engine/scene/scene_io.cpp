#include "scene/scene_io.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace SceneIO {

namespace {

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

} // namespace

std::string resolveScenePath(const std::string& filename, std::string& error)
{
    std::string name = trim(filename);
    if (name.empty()) {
        error = "Filename required";
        return "";
    }
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
        error = "Filename must not contain path separators";
        return "";
    }
    if (name.find("..") != std::string::npos) {
        error = "Filename must not contain '..'";
        return "";
    }

    const std::string ext = ".scene.json";
    if (name.size() < ext.size() ||
        name.compare(name.size() - ext.size(), ext.size(), ext) != 0) {
        name += ext;
    }
    return "assets/scenes/" + name;
}

bool exists(const std::string& filename)
{
    std::string error;
    std::string path = resolveScenePath(filename, error);
    if (path.empty()) {
        return false;
    }
    return std::filesystem::exists(path);
}

std::vector<std::string> listScenes()
{
    std::vector<std::string> scenes;
    const std::string dir = "assets/scenes";
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) {
        return scenes;
    }

    const std::string ext = ".scene.json";
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string name = entry.path().filename().string();
        if (name.size() >= ext.size() &&
            name.compare(name.size() - ext.size(), ext.size(), ext) == 0) {
            scenes.push_back(dir + "/" + name);
        }
    }

    std::sort(scenes.begin(), scenes.end());
    return scenes;
}

} // namespace SceneIO
