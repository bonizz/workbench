#include "capture/capture.h"

#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>

#include <filesystem>

namespace Capture {

namespace {

std::string defaultFilename()
{
    NSDate* now = [NSDate date];
    NSDateFormatter* fmt = [[NSDateFormatter alloc] init];
    [fmt setDateFormat:@"yyyyMMdd_HHmmss"];
    NSString* s = [fmt stringFromDate:now];
    [fmt release];
    return "capture_" + std::string([s UTF8String]) + ".png";
}

} // namespace

std::string makeCapturePath(const std::string& filename)
{
    std::filesystem::path dir = "captures";
    std::filesystem::create_directories(dir);
    std::string name = filename.empty() ? defaultFilename() : filename;
    return (dir / name).string();
}

bool isValidFilename(const std::string& filename)
{
    if (filename.empty()) return false;
    if (filename.find('/') != std::string::npos) return false;
    if (filename.find('\\') != std::string::npos) return false;
    if (filename.find("..") != std::string::npos) return false;
    if (filename == "." || filename == "..") return false;
    return true;
}

bool writePNG(const std::string& path, const uint8_t* rgba, int width, int height)
{
    NSBitmapImageRep* rep = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:nil
        pixelsWide:width
        pixelsHigh:height
        bitsPerSample:8
        samplesPerPixel:4
        hasAlpha:YES
        isPlanar:NO
        colorSpaceName:NSDeviceRGBColorSpace
        bitmapFormat:NSBitmapFormatThirtyTwoBitBigEndian
        bytesPerRow:width * 4
        bitsPerPixel:32];
    if (!rep) {
        return false;
    }

    memcpy([rep bitmapData], rgba, static_cast<size_t>(width) * height * 4);

    NSData* data = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    BOOL ok = NO;
    if (data) {
        ok = [data writeToFile:[NSString stringWithUTF8String:path.c_str()] atomically:YES];
    }

    [rep release];
    return ok == YES;
}

} // namespace Capture
