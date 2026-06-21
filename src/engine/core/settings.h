#pragma once

#include "core/math.h"

#include <string>
#include <unordered_map>

namespace Settings {

// Load the last saved window size. Returns false if no valid settings exist.
bool loadWindowSize(int& width, int& height);

// Save the current window size.
void saveWindowSize(int width, int height);

// Load the last saved window position. Returns false if no valid settings exist.
bool loadWindowPosition(int& x, int& y);

// Save the current window position.
void saveWindowPosition(int x, int y);

// Load editor panel visibility state. The map keys are panel names (without the
// "show_" prefix written to disk). Returns true if any states were loaded.
bool loadEditorWindowStates(std::unordered_map<std::string, bool>& states);

// Save editor panel visibility state. Panel names are written as show_<name>.
void saveEditorWindowStates(const std::unordered_map<std::string, bool>& states);

// Load the last saved camera state. Returns false if no saved camera settings
// exist, in which case out parameters are left unchanged.
bool loadCamera(Vec3& position, Vec3& rotation, float& moveSpeed);

// Save the current camera state.
void saveCamera(const Vec3& position, const Vec3& rotation, float moveSpeed);

// Override the settings file path. Used by tests to avoid touching the real
// settings.txt in the project root.
void setSettingsPath(const std::string& path);

// Directory containing the current settings file. Empty if the settings path
// is just a filename in the current working directory.
std::string settingsDirectory();

} // namespace Settings
