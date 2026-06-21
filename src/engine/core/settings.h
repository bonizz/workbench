#pragma once

namespace Settings {

// Load the last saved window size. Returns false if no valid settings exist.
bool loadWindowSize(int& width, int& height);

// Save the current window size.
void saveWindowSize(int width, int height);

} // namespace Settings
