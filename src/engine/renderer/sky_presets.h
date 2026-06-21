#pragma once

#include "render_types.h"

#include <vector>

// A named sky/light preset. Applying one overwrites both the directional light
// and the procedural sky so the scene reads as a coherent time of day.
struct SkyPreset
{
    const char* name;
    LightSettings light;
    SkySettings   sky;
};

// Fixed, ordered list of built-in presets: Noon, Warm Sunset, Dusk / Blue Hour.
// Returned by const reference; no global mutable state.
const std::vector<SkyPreset>& skyPresets();
