#pragma once

#include <cstddef>
#include <string>

class Scene;
class GameObject;

namespace DebugState {

// Builds a deterministic, human-readable snapshot of engine state.
// Intended for agent inspection and bug reporting.
// Takes a non-const Scene so it can refresh derived world matrices first;
// all callers already hold a non-const Scene.
//
// NOTE: the output format is effectively an API. Several unit tests search it
// by substring and one does an exact round-trip, so changes must be ADDITIVE
// (never reorder/rename existing lines). See docs/conventions.md §7.
std::string build(uint64_t frame,
                  float fps,
                  float frameTimeMs,
                  size_t renderCommandCount,
                  Scene& scene,
                  const GameObject* selected,
                  const std::string& lastScriptPath = {},
                  const std::string& lastCapturePath = {},
                  const std::string& lastBundlePath = {},
                  const std::string* lastAssertionFailure = nullptr);

// Writes text to logs/latest_state.txt, creating logs/ if needed.
void writeToFile(const std::string& text, const std::string& path = "logs/latest_state.txt");

} // namespace DebugState
