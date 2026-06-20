#pragma once

#include <string>

class Scene;

namespace SceneSerializer {

// Saves the scene to a human-readable text file. The camera is not serialized.
bool save(const Scene& scene, const std::string& path, std::string& error);

// Loads the scene from a text file. Existing authored objects are replaced,
// the camera is preserved, and fresh ObjectIds are assigned.
bool load(Scene& scene, const std::string& path, std::string& error);

} // namespace SceneSerializer
