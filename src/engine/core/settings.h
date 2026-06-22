#pragma once

#include <string>
#include <unordered_map>
#include <vector>

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

// Load the last opened scene path. Returns false if none is recorded.
bool loadLastScene(std::string& path);

// Save the last opened scene path.
void saveLastScene(const std::string& path);

// Maximum number of entries kept in the recent-scenes list.
inline constexpr size_t kMaxRecentScenes = 10;

// Load the recently opened/saved scene paths, most-recent first. Returns false
// if none are recorded.
bool loadRecentScenes(std::vector<std::string>& paths);

// Record `path` as the most recent scene: move it to the front of the list,
// drop any duplicate, and cap the list at kMaxRecentScenes entries.
void addRecentScene(const std::string& path);

// Override the settings file path. Used by tests to avoid touching the real
// settings.json in the project root.
void setSettingsPath(const std::string& path);

// Directory containing the current settings file. Empty if the settings path
// is just a filename in the current working directory.
std::string settingsDirectory();

} // namespace Settings
