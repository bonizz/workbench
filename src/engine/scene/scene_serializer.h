#pragma once

#include <string>

class Scene;

namespace SceneSerializer {

// Saves the scene (objects, camera, environment, and settings) to a JSON file.
bool save(const Scene& scene, const std::string& path, std::string& error);

// Loads the scene from a JSON file. Existing authored objects are replaced.
// Camera and environment are populated from the document; missing fields fall
// back to the current scene defaults. Fresh ObjectIds are assigned.
// If warning is non-null, non-fatal issues (e.g. objects with no renderer or
// an unknown future file version) are appended without failing the load.
bool load(Scene& scene, const std::string& path, std::string& error, std::string* warning = nullptr);

// Serializes the scene to a compact JSON string. Used for in-memory undo
// snapshots (SceneHistory). Equivalent to save() without the filesystem.
std::string serialize(const Scene& scene);

// Applies a JSON snapshot (produced by serialize) to the scene, replacing
// authored objects and environment. **Does not touch the camera**: camera
// navigation is excluded from undo, so undo/redo never moves the camera.
// Returns false with `error` filled on a malformed snapshot.
bool deserialize(Scene& scene, const std::string& text, std::string& error);

} // namespace SceneSerializer
