#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

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
};

struct AgentCommandResult
{
    bool success = false;
    std::string output;
};

// Executes a single text command against the provided context.
// Commands are simple, space-separated tokens (e.g. "scene.select 2").
AgentCommandResult executeCommand(const std::string& command, AgentCommandContext& ctx);
