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

} // namespace SceneSerializer
