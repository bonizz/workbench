#pragma once

struct InputState
{
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool up = false;      // E
    bool down = false;    // Q

    float mouseDeltaX = 0.0f;
    float mouseDeltaY = 0.0f;

    float scrollDelta = 0.0f;
};
