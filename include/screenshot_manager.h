#ifndef SCREENSHOT_MANAGER_H
#define SCREENSHOT_MANAGER_H

#include <stdint.h>

#ifndef NATIVE_TEST
#include <Arduino.h>
#include <FS.h>
#else
#include "Arduino.h"
#include <string>
#endif

struct BmpHeader {
    uint8_t bytes[54];
};

class ScreenshotManager {
public:
    // --- BMP utility methods ---
    static BmpHeader createHeader(uint32_t width, uint32_t height);
    static void convertRGB565ToBGR24(uint16_t rgb565, uint8_t& r, uint8_t& g, uint8_t& b);
    static std::string generateFilename(unsigned long fallbackMillis);

    // --- LVGL flush-interception capture (settings screen) ---
    // Call before forcing a full LVGL redraw; opens the SD file and writes header.
    static bool beginCapture(const char* filepath);
    // Called from disp_flush callback for every rendered tile.
    static void onFlushTile(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint16_t* pixels);
    // Called once the full frame has been captured; closes the SD file.
    static void endCapture();
    // Returns true if a LVGL capture is currently in progress.
    static bool isCaptureInProgress();
    // Convenience: trigger a full LVGL capture sequence to SD.
    static bool captureToSD(const char* filepath);

    // --- TFT readRect capture (optimization / raw-TFT screens) ---
    // Reads pixels directly from the TFT framebuffer via readRect() and writes
    // a BMP to SD. Works for any screen drawn with raw tft.* calls.
    static bool captureTFTToSD(const char* filepath);
};

#endif // SCREENSHOT_MANAGER_H
