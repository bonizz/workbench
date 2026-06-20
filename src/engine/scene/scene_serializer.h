#pragma once

#include <string>

class Scene;

namespace SceneSerializer {

// Saves the scene to a human-readable text file. The camera is not serialized.
bool save(const Scene& scene, const std::string& path, std::string& error);

// Loads the scene from a text file. Existing authored objects are replaced,
// the camera is preserved, and fresh ObjectIds are assigned.
// If warning is non-null, non-fatal issues (e.g. objects with no renderer)
// are appended without failing the load.
bool load(Scene& scene, const std::string& path, std::string& error, std::string* warning = nullptr);

} // namespace SceneSerializer
