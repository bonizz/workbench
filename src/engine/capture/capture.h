#pragma once

#include <cstdint>
#include <string>

namespace Capture {

// Returns a full path under captures/.
// If filename is empty, generates capture_YYYYMMDD_HHMMSS.png.
std::string makeCapturePath(const std::string& filename = {});

// Rejects empty strings, path separators, and directory traversal.
bool isValidFilename(const std::string& filename);

// Writes RGBA8 pixels to a PNG file.
bool writePNG(const std::string& path, const uint8_t* rgba, int width, int height);

} // namespace Capture
