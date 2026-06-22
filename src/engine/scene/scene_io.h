#pragma once

#include <string>
#include <vector>

// Path helpers for scene files saved by the editor. All editor-authored scenes
// live directly under assets/scenes/ with a .scene.json extension; these helpers
// centralize that convention so the UI and any future picker agree on it.
namespace SceneIO {

// Resolves a user-entered scene filename to a path under assets/scenes/,
// appending ".scene.json" if it is not already present. Rejects empty names,
// path separators, and ".." so saves cannot escape assets/scenes/. Returns the
// resolved path, or "" with `error` filled on rejection.
std::string resolveScenePath(const std::string& filename, std::string& error);

// True if resolveScenePath(filename) names an existing file. A filename that
// fails to resolve is treated as non-existent.
bool exists(const std::string& filename);

// Lists every "*.scene.json" file directly under assets/scenes/, returning each
// as a project-relative path (e.g. "assets/scenes/arena.scene.json") sorted
// alphabetically. Returns an empty list if the directory does not exist.
std::vector<std::string> listScenes();

} // namespace SceneIO
