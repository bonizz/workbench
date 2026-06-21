#include "core/settings.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

namespace Settings {

namespace {

constexpr const char* kSettingsPath = "settings.txt";

std::string trim(std::string s)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

} // namespace

bool loadWindowSize(int& width, int& height)
{
    if (!std::filesystem::exists(kSettingsPath)) {
        return false;
    }

    std::ifstream file(kSettingsPath);
    if (!file) {
        return false;
    }

    bool gotWidth = false;
    bool gotHeight = false;

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

        try {
            if (key == "window_width") {
                width = std::stoi(value);
                gotWidth = true;
            } else if (key == "window_height") {
                height = std::stoi(value);
                gotHeight = true;
            }
        } catch (...) {
            // Ignore malformed values.
        }
    }

    return gotWidth && gotHeight;
}

void saveWindowSize(int width, int height)
{
    std::ofstream file(kSettingsPath);
    if (!file) {
        return;
    }

    file << "# Workbench settings\n";
    file << "window_width=" << width << "\n";
    file << "window_height=" << height << "\n";
}

} // namespace Settings
