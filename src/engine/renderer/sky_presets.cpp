#include "renderer/sky_presets.h"

// LightSettings.direction is the direction light travels (see docs/conventions.md
// "Directional light convention"); the visible sun sits at -direction. Sunset and
// dusk therefore use a near-horizontal travel direction so the sun rides low.
const std::vector<SkyPreset>& skyPresets()
{
    static const std::vector<SkyPreset> presets = {
        {
            "Noon",
            // direction, ambient, diffuse
            {{-0.3f, -1.0f, -0.2f}, 0.30f, 1.00f},
            {
                {0.55f, 0.65f, 0.78f},  // horizon
                {0.12f, 0.22f, 0.45f},  // zenith
                {1.00f, 0.95f, 0.85f},  // sun
                0.010f,                 // sunSize (rad)
                1.00f,                  // sunIntensity
            },
        },
        {
            "Warm Sunset",
            {{-0.9f, -0.15f, -0.3f}, 0.25f, 0.90f},
            {
                {0.95f, 0.45f, 0.25f},  // horizon — warm orange
                {0.20f, 0.15f, 0.35f},  // zenith — dusky violet
                {1.00f, 0.70f, 0.45f},  // sun — warm
                0.020f,
                1.50f,
            },
        },
        {
            "Dusk / Blue Hour",
            {{-0.6f, -0.25f, -0.4f}, 0.35f, 0.40f},
            {
                {0.18f, 0.22f, 0.38f},  // horizon — deep blue
                {0.05f, 0.08f, 0.18f},  // zenith — near night
                {0.70f, 0.75f, 0.95f},  // sun — cool
                0.015f,
                0.60f,
            },
        },
    };
    return presets;
}
