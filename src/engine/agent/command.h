#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class MetalRenderer;
class Scene;
class GameObject;

// Context bundles the mutable editor state and diagnostic inputs that
// commands need. It is intentionally a plain struct with no ownership.
struct AgentCommandContext
{
    Scene& scene;
    GameObject*& selected;

    uint64_t frame = 0;
    float fps = 0.0f;
    float frameTimeMs = 0.0f;
    size_t renderCommandCount = 0;

    // Updated by script.run so debug.dump can report it.
    std::string lastScriptPath;

    // Optional renderer handle for render.capture.
    MetalRenderer* renderer = nullptr;

    // Updated by render.capture so debug.dump can report it.
    std::string lastCapturePath;

    // Updated by debug.bundle so debug.dump can report it.
    std::string lastBundlePath;

    // Optional pointer to the owner's persistent last assertion failure message.
    // Assertion commands set this on failure so it survives across contexts/frames.
    std::string* lastAssertionFailure = nullptr;
};

struct AgentCommandResult
{
    bool success = false;
    std::string output;
};

// Executes a single text command against the provided context.
// Commands are simple, space-separated tokens (e.g. "scene.select 2").
AgentCommandResult executeCommand(const std::string& command, AgentCommandContext& ctx);
