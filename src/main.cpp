#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <unordered_map>
#include "config/config.h"
#include "file_cache.h"
#include "slideshow_timer.h"
#include "touch_manager.h"
#include "touch_handler.h"
#include "lvgl_manager.h"
#include "hardware_logic.h"
#include "backlight_fader.h"
#include "screenshot_manager.h"
#include "led_manager.h"

BacklightFader fader;
bool transitionPending = false;
enum TransitionDirection { DIR_NEXT, DIR_PREV };
TransitionDirection pendingDirection = DIR_NEXT;

// Initialize the TFT object. 
// Note: Pins and drivers are automatically handled by platformio.ini build_flags!
TFT_eSPI tft = TFT_eSPI();
FileCache fileCache(1024);
SlideshowTimer slideshowTimer(DEFAULT_SLIDESHOW_DELAY_MS);
TouchHandler touchHandler(TFT_HEIGHT, TFT_WIDTH);

int currentBrightness = DEFAULT_BRIGHTNESS;
bool showFilename = DEFAULT_SHOW_FILENAME;
bool isRandomMode = DEFAULT_RANDOM_MODE;
bool isAutoBrightness = DEFAULT_AUTO_BRIGHTNESS;
bool isInactivitySleep = DEFAULT_INACTIVITY_SLEEP;
bool bypassOptimization = DEFAULT_BYPASS_OPTIMIZATION;
bool optimizationCancelled = false;
int currentOrientation = DEFAULT_SCREEN_ORIENTATION; // 1 = Landscape, 2 = Portrait, 3 = Landscape Rev, 4 = Portrait Rev

// CYD RGB LED — pins: R=4, G=16, B=17 (active-LOW, common-anode)
LedManager led(RGB_LED_RED_PIN, RGB_LED_GREEN_PIN, RGB_LED_BLUE_PIN);
int currentLedBrightness = DEFAULT_LED_BRIGHTNESS;
bool isLedEnabled = DEFAULT_LED_ENABLED;

bool isCancelButtonTouched(int touchX, int touchY);
void drawCancellingFeedback();
bool renderScaledJpg(const char* filename);
void handleClearCache();

#include "catppuccin.h"

int currentThemeFlavor = DEFAULT_THEME_FLAVOR;

bool isWifiEnabled = false;
std::string wifiSSID = DEFAULT_WIFI_SSID;
std::string wifiPassword = DEFAULT_WIFI_PASSWORD;
#include "wifi_manager.h"
WifiManager* wifiManager = nullptr;

#include "app_state.h"
AppState currentState = STATE_SLIDESHOW;

bool isSleeping = false;
unsigned long lastTouchTimeMs = 0;

float current_scale_sw = 1.0f;
int16_t current_x_offset = 0;
int16_t current_y_offset = 0;

// SD Card Chip Select for CYD
// const uint8_t SD_CS_PIN = 5;

// FNV-1a 64-bit hash for memory-efficient cache inventory filename lookups.
// Prevents dynamic string heap allocations and fragmentation under high file counts.
inline uint64_t fnv1a_hash(const char* str) {
  uint64_t hash = 14695981039346656037ULL;
  while (*str) {
    hash ^= (uint64_t)(unsigned char)*str++;
    hash *= 1099511628211ULL;
  }
  return hash;
}

// Dynamic RGB888 to RGB565 conversion macro
#define RGB888_TO_RGB565(c) ((((c) & 0xF80000) >> 8) | (((c) & 0xFC00) >> 5) | (((c) & 0xF8) >> 3))

#define CTP_BASE     RGB888_TO_RGB565(getCatppuccinFlavor(currentThemeFlavor).base)
#define CTP_RED      RGB888_TO_RGB565(getCatppuccinFlavor(currentThemeFlavor).red)
#define CTP_TEXT     RGB888_TO_RGB565(getCatppuccinFlavor(currentThemeFlavor).text)
#define CTP_GREEN    RGB888_TO_RGB565(getCatppuccinFlavor(currentThemeFlavor).green)
#define CTP_SURFACE0 RGB888_TO_RGB565(getCatppuccinFlavor(currentThemeFlavor).overlay)
#define CTP_MANTLE   RGB888_TO_RGB565(getCatppuccinFlavor(currentThemeFlavor).mantle)

std::string getCachePath(const std::string& originalPath) {
  size_t lastDot = originalPath.find_last_of('.');
  std::string base = (lastDot == std::string::npos) ? originalPath : originalPath.substr(0, lastDot);
  if (base.rfind("/", 0) == 0) {
    base = base.substr(1);
  }
  return "/cache/" + base + "_" + std::to_string(tft.width()) + "x" + std::to_string(tft.height()) + ".raw";
}

bool renderRawImage(const char* filename) {
  File rawFile = SD.open(filename, FILE_READ);
  if (!rawFile) {
    Serial.printf("Failed to open raw image: %s\n", filename);
    return false;
  }
  
  // Allocate buffer on the heap to avoid stack overflow
  const size_t bufferSize = tft.width() * 16;
  uint16_t* buffer = (uint16_t*)malloc(bufferSize * sizeof(uint16_t));
  if (!buffer) {
    Serial.println("Failed to allocate buffer for raw image rendering!");
    rawFile.close();
    return false;
  }
  
  tft.startWrite();
  tft.setAddrWindow(0, 0, tft.width(), tft.height());
  
  while (rawFile.available()) {
    int readCount = rawFile.read((uint8_t*)buffer, bufferSize * sizeof(uint16_t));
    if (readCount <= 0) break;
    tft.pushPixels(buffer, readCount / sizeof(uint16_t));
  }
  tft.endWrite();
  free(buffer);
  rawFile.close();
  return true;
}

File cacheFile;
bool cacheFileActive = false;
// PSRAM-backed frame buffer used during cache generation.
// Decoded into RAM, then flushed in one sequential write — avoids per-row SD seeks.
uint16_t* g_frameBuffer = nullptr;
size_t    g_frameBufferSize = 0;

/**
 * @brief Callback function required by TJpg_Decoder.
 * Pushes decompressed MCU blocks (usually 16x16 pixels) to the screen.
 */
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (optimizationCancelled) {
    return 0;
  }

  // Poll touch screen frequently during decoding (every 5 MCU blocks) to check for Cancel
  static int blockCounter = 0;
  blockCounter++;
  if (blockCounter >= 5) {
    blockCounter = 0;
    LVGLManager::handle();
    if (optimizationCancelled) {
      Serial.println("[System] Optimization cancelled during decode (via LVGL callback).");
      drawCancellingFeedback();
      return 0;
    }
    if (TouchManager::isTouched()) {
      int tx = 0, ty = 0;
      if (TouchManager::getTouchPoint(tx, ty)) {
        Serial.printf("[Touch Debug] Touch detected during decode: Raw X=%d, Y=%d\n", tx, ty);
        if (isCancelButtonTouched(tx, ty)) {
          Serial.println("[System] Optimization CANCELLED by user touch during file decode!");
          optimizationCancelled = true;
          drawCancellingFeedback();
          return 0; // Abort TJpgDec decoding
        }
      }
    }
  }

  if (optimizationCancelled) {
    return 0;
  }

  // Calculate block bounds in scaled coordinate system (using rounding to prevent gaps)
  uint16_t x_start_scaled = (uint16_t)round((float)x / current_scale_sw);
  uint16_t x_end_scaled = (uint16_t)round((float)(x + w) / current_scale_sw);
  uint16_t w_scaled = x_end_scaled - x_start_scaled;

  uint16_t y_start_scaled = (uint16_t)round((float)y / current_scale_sw);
  uint16_t y_end_scaled = (uint16_t)round((float)(y + h) / current_scale_sw);
  uint16_t h_scaled = y_end_scaled - y_start_scaled;

  if (w_scaled == 0 || h_scaled == 0) return 1;

  int16_t dest_x = current_x_offset + x_start_scaled;
  int16_t dest_y = current_y_offset + y_start_scaled;

  if (dest_y >= tft.height()) return 0;
  
  int16_t w_visible = w_scaled;
  if (dest_x >= tft.width()) return 1;
  if (dest_x + w_visible > tft.width()) {
    w_visible = tft.width() - dest_x;
  }
  
  int16_t h_visible = h_scaled;
  if (dest_y + h_visible > tft.height()) {
    h_visible = tft.height() - dest_y;
  }

  // Bypass software scaling completely if scale is 1
  if (current_scale_sw == 1.0f) {
    if (!cacheFileActive) {
      tft.pushImage(dest_x, dest_y, w_visible, h_visible, bitmap, w);
    } else {
      if (g_frameBuffer) {
        // Fast path: copy directly into the PSRAM frame buffer (no SD I/O).
        for (int row = 0; row < h_visible; row++) {
          size_t fbOffset = (size_t)(dest_y + row) * tft.width() + dest_x;
          memcpy(g_frameBuffer + fbOffset, bitmap + row * w, w_visible * 2);
        }
      } else if (cacheFile) {
        for (int row = 0; row < h_visible; row++) {
          unsigned long fileOffset = ((dest_y + row) * tft.width() + dest_x) * 2;
          cacheFile.seek(fileOffset);
          cacheFile.write((uint8_t*)(bitmap + row * w), w_visible * 2);
        }
      }
    }
    return 1;
  }

  // Software scale is != 1.0f. Render row by row to support downscaling & upscaling.
  uint16_t line_buffer[480];
  uint16_t safe_w_visible = w_visible > 480 ? 480 : w_visible;
  uint16_t safe_h_visible = h_visible > 480 ? 480 : h_visible;

  for (uint16_t row = 0; row < safe_h_visible; row++) {
    int16_t src_y = (int16_t)floor(((float)(y_start_scaled + row) + 0.5f) * current_scale_sw) - y;
    if (src_y < 0) src_y = 0;
    if (src_y >= h) src_y = h - 1;
    
    for (uint16_t col = 0; col < safe_w_visible; col++) {
      int16_t src_x = (int16_t)floor(((float)(x_start_scaled + col) + 0.5f) * current_scale_sw) - x;
      if (src_x < 0) src_x = 0;
      if (src_x >= w) src_x = w - 1;
      
      line_buffer[col] = bitmap[src_y * w + src_x];
    }

    if (!cacheFileActive) {
      tft.pushImage(dest_x, dest_y + row, safe_w_visible, 1, line_buffer);
    } else {
      if (g_frameBuffer) {
        // Fast path: copy scaled row into the PSRAM frame buffer.
        size_t fbOffset = (size_t)(dest_y + row) * tft.width() + dest_x;
        memcpy(g_frameBuffer + fbOffset, line_buffer, safe_w_visible * 2);
      } else if (cacheFile) {
        unsigned long fileOffset = ((dest_y + row) * tft.width() + dest_x) * 2;
        cacheFile.seek(fileOffset);
        cacheFile.write((uint8_t*)line_buffer, safe_w_visible * 2);
      }
    }
  }
  return 1;
}

bool generateCache(const char* jpgFilename, const char* rawFilename) {
  Serial.printf("[System] Optimizing: %s\n", jpgFilename);
  std::string tempFilename = std::string(rawFilename) + ".tmp";

  // ------------------------------------------------------------------
  // Allocate a PSRAM-backed full-frame buffer so the entire image can
  // be decoded into RAM and then written to the SD card in a single
  // sequential write — avoiding hundreds of per-row seek+write calls.
  // ------------------------------------------------------------------
  const size_t frameBytes = (size_t)tft.width() * (size_t)tft.height() * 2;
  if (g_frameBuffer == nullptr || g_frameBufferSize != frameBytes) {
    if (g_frameBuffer) { free(g_frameBuffer); g_frameBuffer = nullptr; }
    // Prefer PSRAM (ps_malloc) for the large frame buffer; fall back to heap.
    g_frameBuffer = (uint16_t*)ps_malloc(frameBytes);
    if (!g_frameBuffer) {
      g_frameBuffer = (uint16_t*)malloc(frameBytes);
    }
    if (g_frameBuffer) { g_frameBufferSize = frameBytes; }
  }

  const bool useFrameBuf = (g_frameBuffer != nullptr);

  if (useFrameBuf) {
    // Pre-fill frame buffer with the background color.
    uint16_t swapped_base = (CTP_BASE >> 8) | (CTP_BASE << 8);
    uint16_t* p = g_frameBuffer;
    uint16_t* end = p + (frameBytes / 2);
    while (p < end) { *p++ = swapped_base; }
  } else {
    // Fallback: write directly to SD (original row-by-row method).
    Serial.println("[System] WARNING: No RAM for frame buffer; falling back to SD seek-writes.");
    if (SD.exists(tempFilename.c_str())) {
      SD.remove(tempFilename.c_str());
    }
    cacheFile = SD.open(tempFilename.c_str(), FILE_WRITE);
    if (!cacheFile) {
      Serial.printf("Failed to create cache file: %s\n", tempFilename.c_str());
      return false;
    }
    const size_t rowSize = tft.width();
    uint16_t* rowBuffer = (uint16_t*)malloc(rowSize * sizeof(uint16_t));
    if (!rowBuffer) {
      Serial.println("Failed to allocate preallocation row buffer!");
      cacheFile.close();
      return false;
    }
    uint16_t swapped_base = (CTP_BASE >> 8) | (CTP_BASE << 8);
    for (size_t i = 0; i < rowSize; i++) { rowBuffer[i] = swapped_base; }
    for (int i = 0; i < tft.height(); i++) {
      cacheFile.write((uint8_t*)rowBuffer, rowSize * sizeof(uint16_t));
    }
    free(rowBuffer);
  }

  uint16_t img_w = 0, img_h = 0;
  uint16_t result = TJpgDec.getSdJpgSize(&img_w, &img_h, jpgFilename);
  if (result != 0) {
    Serial.printf("Failed to read header for caching: %s\n", jpgFilename);
    if (!useFrameBuf) {
      cacheFile.close();
      if (SD.exists(tempFilename.c_str())) { SD.remove(tempFilename.c_str()); }
    }
    return false;
  }

  float ratio_w = (float)img_w / (float)tft.width();
  float ratio_h = (float)img_h / (float)tft.height();
  float max_ratio = max(ratio_w, ratio_h);

  uint8_t scale_hw = 1;
  float scale_sw = 1.0f;

  if (max_ratio < 2.0f) {
    scale_hw = 1;
  } else if (max_ratio < 4.0f) {
    scale_hw = 2;
  } else if (max_ratio < 8.0f) {
    scale_hw = 4;
  } else {
    scale_hw = 8;
  }

  scale_sw = max_ratio / (float)scale_hw;

  TJpgDec.setJpgScale(scale_hw);

  int16_t scaled_w = (int16_t)round((float)img_w / ((float)scale_hw * scale_sw));
  int16_t scaled_h = (int16_t)round((float)img_h / ((float)scale_hw * scale_sw));
  int16_t x_offset = (tft.width() - scaled_w) / 2;
  int16_t y_offset = (tft.height() - scaled_h) / 2;

  current_scale_sw = scale_sw;
  current_x_offset = x_offset;
  current_y_offset = y_offset;

  cacheFileActive = true;
  uint16_t drawResult = TJpgDec.drawSdJpg(0, 0, jpgFilename);
  cacheFileActive = false;

  if (!useFrameBuf) {
    cacheFile.close();
  }

  if (drawResult != 0 && drawResult != 1) {
    Serial.printf("Error during caching decode: %d\n", drawResult);
    if (!useFrameBuf && SD.exists(tempFilename.c_str())) {
      SD.remove(tempFilename.c_str());
    }
    return false;
  }

  if (useFrameBuf) {
    // Write the fully-decoded frame buffer to SD in one sequential pass.
    if (SD.exists(tempFilename.c_str())) {
      SD.remove(tempFilename.c_str());
    }
    File outFile = SD.open(tempFilename.c_str(), FILE_WRITE);
    if (!outFile) {
      Serial.printf("Failed to create cache file: %s\n", tempFilename.c_str());
      return false;
    }
    const size_t chunkBytes = 4096;
    const uint8_t* src = (const uint8_t*)g_frameBuffer;
    size_t remaining = frameBytes;
    while (remaining > 0) {
      size_t toWrite = remaining < chunkBytes ? remaining : chunkBytes;
      outFile.write(src, toWrite);
      src += toWrite;
      remaining -= toWrite;
    }
    outFile.close();
  }

  // Rename temp file to final destination
  if (SD.exists(rawFilename)) {
    SD.remove(rawFilename); // Remove old cached file if it exists
  }
  if (!SD.rename(tempFilename.c_str(), rawFilename)) {
    Serial.printf("Failed to rename cached file: %s -> %s\n", tempFilename.c_str(), rawFilename);
    if (SD.exists(tempFilename.c_str())) {
      SD.remove(tempFilename.c_str());
    }
    return false;
  }

  return true;
}

void drawCancelButton() {
  int btnW = 100;
  int btnH = 30;
  int btnX = (tft.width() - btnW) / 2;
  int btnY = tft.height() - 40;
  
  tft.fillRect(btnX, btnY, btnW, btnH, CTP_RED);
  tft.setTextColor(CTP_BASE, CTP_RED);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Cancel", btnX + btnW / 2, btnY + btnH / 2, 2);
}

static int mapRangeClippedMain(int val, int in_min, int in_max, int out_max) {
  if (in_max == in_min) return 0;
  long out = (long)(val - in_min) * out_max / (in_max - in_min);
  if (out < 0) return 0;
  if (out > out_max) return out_max;
  return (int)out;
}

void mapTouchPoint(int rawX, int rawY, int& pixelX, int& pixelY) {
  bool isCapacitive = (rawX <= tft.width() * 2 && rawY <= tft.height() * 2);
  if (isCapacitive) {
    int w_land = max(tft.width(), tft.height());
    int h_land = min(tft.width(), tft.height());
    
    bool rawIsPortrait = (rawX <= h_land && rawY <= w_land);
    
    if (rawIsPortrait) {
      switch (currentOrientation) {
        case 0: // Portrait Rev / Native Portrait
          pixelX = h_land - rawX;
          pixelY = rawY;
          break;
        case 1: // Landscape
          pixelX = rawY;
          pixelY = h_land - rawX;
          break;
        case 2: // Portrait
          pixelX = rawX;
          pixelY = w_land - rawY;
          break;
        case 3: // Landscape Rev
          pixelX = w_land - rawY;
          pixelY = rawX;
          break;
        default:
          pixelX = rawY;
          pixelY = rawX;
          break;
      }
    } else {
      switch (currentOrientation) {
        case 0: // Portrait
          pixelX = rawY;
          pixelY = w_land - rawX;
          break;
        case 1: // Landscape
          pixelX = rawX;
          pixelY = rawY;
          break;
        case 2: // Portrait Rev
          pixelX = h_land - rawY;
          pixelY = rawX;
          break;
        case 3: // Landscape Rev
          pixelX = w_land - rawX;
          pixelY = h_land - rawY;
          break;
        default:
          pixelX = rawX;
          pixelY = rawY;
          break;
      }
    }
    
    if (pixelX < 0) pixelX = 0;
    if (pixelX >= tft.width()) pixelX = tft.width() - 1;
    if (pixelY < 0) pixelY = 0;
    if (pixelY >= tft.height()) pixelY = tft.height() - 1;
    return;
  }
  // Resistive mapping:
  int w_land = max(tft.width(), tft.height());
  int h_land = min(tft.width(), tft.height());
  
  int lx = mapRangeClippedMain(rawX, 200, 3800, w_land);
  int ly = mapRangeClippedMain(rawY, 200, 3800, h_land);
  
  switch (currentOrientation) {
    case 0: // Portrait Rev
      pixelX = h_land - ly;
      pixelY = lx;
      break;
    case 1: // Landscape
      pixelX = lx;
      pixelY = ly;
      break;
    case 2: // Portrait
      pixelX = ly;
      pixelY = w_land - lx;
      break;
    case 3: // Landscape Rev
      pixelX = w_land - lx;
      pixelY = h_land - ly;
      break;
    default:
      pixelX = lx;
      pixelY = ly;
      break;
  }
}

bool isCancelButtonTouched(int rawX, int rawY) {
  int pixelX = 0;
  int pixelY = 0;
  mapTouchPoint(rawX, rawY, pixelX, pixelY);

  Serial.printf("[Touch Debug] Cancel Screen Tap: Raw(%d,%d) -> Mapped Pixel(%d,%d) => MATCH (CANCEL TRIGGERED)\n",
                rawX, rawY, pixelX, pixelY);

  return true; // Any tap anywhere on screen while in calculating/optimizing mode triggers cancellation
}

void restrictCacheToExisting() {
  std::vector<std::string> validFiles;
  for (size_t i = 0; i < fileCache.size(); i++) {
    std::string originalPath = fileCache.getAt(i);
    std::string cachePath = getCachePath(originalPath);
    bool cacheValid = false;
    if (SD.exists(cachePath.c_str())) {
      File checkFile = SD.open(cachePath.c_str(), FILE_READ);
      if (checkFile) {
        if (checkFile.size() == (size_t)(tft.width() * tft.height() * 2)) {
          cacheValid = true;
        }
        checkFile.close();
      }
    }
    if (cacheValid) {
      validFiles.push_back(originalPath);
    }
  }

  fileCache.clear();
  for (const auto& path : validFiles) {
    fileCache.addFile(path);
  }
  Serial.printf("[System] Restricted cache to %zu already-cached images.\n", fileCache.size());
}

void drawCalculating() {
  LVGLManager::showOptimizationScreen();
  LVGLManager::setCancelCallback([]() {
    optimizationCancelled = true;
  });
}

void drawCancellingFeedback() {
  LVGLManager::setOptimizationCancelling();
}

void drawProgress(size_t current, size_t total, const char* filename) {
  LVGLManager::updateOptimizationProgress(current, total, filename);
}

/**
 * @brief Displays an error state if the SD card is missing or unreadable.
 */
void showSDError() {
  LVGLManager::showSDError();
  Serial.println("Error: SD Card Mount Failed.");
  while(true) {
    LVGLManager::handle();
    delay(10);
  }
}

static bool toastActive = false;
static unsigned long toastEndTimeMs = 0;
static char toastMessage[64] = {0};

void drawToastBanner(const char* message) {
  tft.fillRect(0, 0, tft.width(), 28, CTP_MANTLE);
  tft.drawFastHLine(0, 27, tft.width(), CTP_GREEN);
  tft.setTextColor(CTP_TEXT, CTP_MANTLE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(message, tft.width() / 2, 14, 2);
}

void showToastBanner(const char* message, unsigned long durationMs = 2000) {
  if (currentState != STATE_SLIDESHOW) return;
  
  strncpy(toastMessage, message, sizeof(toastMessage) - 1);
  toastMessage[sizeof(toastMessage) - 1] = '\0';
  
  drawToastBanner(toastMessage);
  
  toastActive = true;
  toastEndTimeMs = millis() + durationMs;
}

void drawToastBannerIfNeeded() {
  if (toastActive && millis() < toastEndTimeMs) {
    drawToastBanner(toastMessage);
  }
}

void clearToastBanner() {
  if (!toastActive) return;
  toastActive = false;
  
  if (currentState == STATE_SLIDESHOW && !fileCache.isEmpty()) {
    renderScaledJpg(fileCache.getCurrent().c_str());
  }
}

void drawFilenameBanner(const char* filename) {
  const char* namePtr = strrchr(filename, '/');
  const char* displayName = namePtr ? namePtr + 1 : filename;
  
  // Draw solid Catppuccin Mantle background banner
  tft.fillRect(0, tft.height() - 24, tft.width(), 24, CTP_MANTLE);
  
  // Draw text centered in the banner
  tft.setTextColor(CTP_TEXT, CTP_MANTLE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(displayName, tft.width() / 2, tft.height() - 12, 2);
}

/**
 * @brief Reads the JPEG header, determines the optimal hardware scale 
 * factor (1, 2, 4, or 8), calculates centering offsets, and renders it.
 * @return true if drawing succeeded, false otherwise.
 */
bool renderScaledJpg(const char* filename) {
  std::string cachePath = getCachePath(filename);
  if (SD.exists(cachePath.c_str())) {
    bool cacheValid = false;
    File checkFile = SD.open(cachePath.c_str(), FILE_READ);
    if (checkFile) {
      if (checkFile.size() == (size_t)(tft.width() * tft.height() * 2)) {
        cacheValid = true;
      }
      checkFile.close();
    }
    
    if (cacheValid) {
      Serial.printf("[System] Loading cached raw image: %s\n", cachePath.c_str());
      bool drawSuccess = renderRawImage(cachePath.c_str());
      if (drawSuccess) {
        if (showFilename) {
          drawFilenameBanner(filename);
        }
        drawToastBannerIfNeeded();
        return true;
      } else {
        Serial.println("[System] Failed to render raw cache, falling back to decoding original JPEG...");
      }
    } else {
      Serial.printf("[System] Deleting invalid/corrupt cache file: %s\n", cachePath.c_str());
      if (SD.exists(cachePath.c_str())) {
        SD.remove(cachePath.c_str());
      }
    }
  }

  uint16_t img_w = 0, img_h = 0;

  Serial.printf("\n--- Rendering: %s ---\n", filename);

  // 1. Read the JPEG header (Returns 0 on success)
  uint16_t result = TJpgDec.getSdJpgSize(&img_w, &img_h, filename);
  
  if (result == 0) {
    float ratio_w = (float)img_w / tft.width();
    float ratio_h = (float)img_h / tft.height();
    float max_ratio = max(ratio_w, ratio_h);

    uint8_t scale_hw = 1;
    float scale_sw = 1.0f;

    if (max_ratio < 2.0f) {
      scale_hw = 1;
    } else if (max_ratio < 4.0f) {
      scale_hw = 2;
    } else if (max_ratio < 8.0f) {
      scale_hw = 4;
    } else {
      scale_hw = 8;
    }

    scale_sw = max_ratio / (float)scale_hw;

    Serial.printf("Original Size: %dx%d | Screen Size: %dx%d | HW Scale: 1/%d | SW Scale: 1/%.2f\n", 
                  img_w, img_h, tft.width(), tft.height(), scale_hw, scale_sw);

    TJpgDec.setJpgScale(scale_hw);

    int16_t scaled_w = (int16_t)round((float)img_w / ((float)scale_hw * scale_sw));
    int16_t scaled_h = (int16_t)round((float)img_h / ((float)scale_hw * scale_sw));
    
    int16_t x_offset = (tft.width() - scaled_w) / 2;
    int16_t y_offset = (tft.height() - scaled_h) / 2;

    current_scale_sw = scale_sw;
    current_x_offset = x_offset;
    current_y_offset = y_offset;

    tft.fillScreen(CTP_BASE);

    uint16_t drawResult = TJpgDec.drawSdJpg(0, 0, filename);
    if (drawResult != 0 && drawResult != 1) {
      Serial.printf("Error during draw: JRESULT %d\n", drawResult);
      return false;
    } else {
      if (showFilename) {
        drawFilenameBanner(filename);
      }
      drawToastBannerIfNeeded();
      return true;
    }
    
  } else {
    // If it fails, print the exact error code
    Serial.printf("Error: Could not read JPEG header. JRESULT Code: %d\n", result);
    
    // Some common error codes translated:
    if (result == 3) Serial.println("-> JDR_MEM1: Not enough RAM");
    if (result == 5) Serial.println("-> JDR_FMT1: Invalid JPEG format (Is it a progressive JPEG?)");
    
    return false;
  }
}

void populateCache() {
  fileCache.clear();
  File root = SD.open("/");
  if (!root) {
    Serial.println("Error: Failed to open root directory.");
    return;
  }
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String filePath = String(file.path());
      String fileNameLower = filePath;
      fileNameLower.toLowerCase();
      if (fileNameLower.endsWith(".jpg") || fileNameLower.endsWith(".jpeg")) {
        if (!fileCache.addFile(filePath.c_str())) {
          Serial.println("[Cache] Full.");
          file.close();
          break;
        }
      }
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
}

void saveConfig() {
  Preferences prefs;
  prefs.begin("settings", false);
  HardwareLogic::saveSettings(prefs, currentBrightness, isAutoBrightness, slideshowTimer.getInterval(), isRandomMode, showFilename, isInactivitySleep, currentThemeFlavor, currentOrientation, currentLedBrightness, isLedEnabled, isWifiEnabled, wifiSSID, wifiPassword, bypassOptimization);
  prefs.end();
  Serial.println("[System] Settings saved to NVS.");
}

bool pendingExitSettings = false;
void triggerExitSettings() {
  pendingExitSettings = true;
}

void exitSettings() {
  int cachedTheme = 0;
  int cachedOrientation = 1;
  bool cachedWifiEnabled = false;
  {
    Preferences prefs;
    prefs.begin("settings", false);
    cachedTheme = (int)prefs.getUInt("cached_theme", 0);
    cachedOrientation = (int)prefs.getInt("cached_rot", 1);
    cachedWifiEnabled = prefs.getBool("cached_wifi", false);
    prefs.end();
  }

  saveConfig();

#if !defined(NATIVE_TEST)
  if (currentThemeFlavor != cachedTheme || currentOrientation != cachedOrientation || isWifiEnabled != cachedWifiEnabled) {
    Serial.println("[System] Theme flavor, orientation, or WiFi setting changed. Rebooting...");
    delay(500);
    ESP.restart();
  }
#endif

  // Clear settings screen in LVGL and delete the objects while still in STATE_SETTINGS
  LVGLManager::hideSettings();
  
#if !defined(NATIVE_TEST)
  // Tick LVGL twice to ensure it cleans up the objects and renders the transparent screen
  LVGLManager::handle();
  delay(10);
  LVGLManager::handle();
#endif

  // Transition to slideshow state
  currentState = STATE_SLIDESHOW;
  led.setState(LedManager::STATE_SLIDESHOW);
  slideshowTimer.setPaused(false);
  slideshowTimer.reset(millis());
  Serial.println("[System] Exiting settings menu. Resuming slideshow.");

  // Render transition screen
  tft.fillScreen(CTP_BASE);
  tft.setTextColor(CTP_TEXT, CTP_BASE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("CYD Photo Frame", tft.width() / 2, tft.height() / 2 - 20, 4);
  tft.setTextColor(CTP_GREEN, CTP_BASE);
  tft.drawString("Resuming slideshow...", tft.width() / 2, tft.height() / 2 + 20, 2);

  delay(500);

  // Smoothly fade out "Resuming slideshow..." screen to black
  fader.startFade(currentBrightness, 0, 250);
  while (fader.getState() != BacklightFader::STATE_IDLE) {
    int b = 0;
    fader.update(millis(), b);
#if defined(TFT_BL) && (TFT_BL >= 0)
    analogWrite(TFT_BL, b);
#endif
    delay(10);
  }
#if defined(TFT_BL) && (TFT_BL >= 0)
  analogWrite(TFT_BL, 0);
#endif

  // Render the photo in complete darkness (no line scan visible!)
  tft.fillScreen(CTP_BASE);
  if (!fileCache.isEmpty()) {
    renderScaledJpg(fileCache.getCurrent().c_str());
  }

  // Smoothly fade in to the rendered photo
  transitionPending = true;
  fader.startFade(0, currentBrightness, 300);
}

void handleClearCache() {
  Serial.println("[System] Clear cache requested. Commencing deletion...");
  
  std::vector<std::string> filesToDelete;
  File cacheDir = SD.open("/cache");
  if (cacheDir) {
    File file = cacheDir.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        filesToDelete.push_back(std::string(file.path()));
      }
      file.close();
      file = cacheDir.openNextFile();
    }
    cacheDir.close();
  }

  size_t total = filesToDelete.size();
  Serial.printf("[System] Found %zu files to delete from cache.\n", total);
  
  LVGLManager::showClearCacheScreen();
  led.setState(LedManager::STATE_OPTIMIZING);

  if (total == 0) {
    LVGLManager::updateClearCacheProgress(0, 0, "No cached files found.");
    delay(3000);
  } else {
    for (size_t i = 0; i < total; i++) {
      const char* path = filesToDelete[i].c_str();
      const char* displayName = strrchr(path, '/');
      displayName = displayName ? displayName + 1 : path;
      
      LVGLManager::updateClearCacheProgress(i + 1, total, displayName);
      
      if (SD.exists(path)) {
        SD.remove(path);
      }
      
      // Delay slightly (e.g. 50ms) to show smooth progress UI
      delay(50);
    }
    LVGLManager::updateClearCacheProgress(total, total, "Rebooting to rebuild cache...");
    delay(3000);
  }

  LVGLManager::hideClearCacheScreen();
  
  Serial.println("[System] Cache cleared. Rebooting now...");
  delay(100);
#if !defined(NATIVE_TEST)
  ESP.restart();
#endif
}

void setup() {
  Serial.begin(115200);
  Serial.println("[System] Booting ESP32 CYD Photo Frame...");
  Serial.println("[System] Serial commands: 'clear'/'clear_cache', 'screenshot' (settings screen), 'screenshot_tft' (current raw TFT screen)");

#if defined(SD_CS_PIN) && (SD_CS_PIN >= 0)
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delay(800); // Allow SD card power rails & internal controller to stabilize on cold boot
#endif

  // Initialize SD Card FIRST (before TFT, Touch, and LVGL SPI bus initialization)
  Serial.println("Mounting SD Card...");
  bool sdSuccess = false;
  uint32_t frequencies[] = {25000000UL, 20000000UL, 10000000UL, 4000000UL};
  
#if defined(SD_SCK_PIN) && defined(SD_MISO_PIN) && defined(SD_MOSI_PIN) && defined(SD_CS_PIN)
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
#else
  SPI.begin();
#endif

  // Send 80 dummy clock cycles with CS high to initialize SD card SPI mode (per SD specification)
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  for (int i = 0; i < 10; i++) {
    SPI.transfer(0xFF);
  }
  SPI.endTransaction();
  delay(50);

  for (uint32_t freq : frequencies) {
    for (int retry = 1; retry <= 2; retry++) {
      if (SD.begin(SD_CS_PIN, SPI, freq, "/sd", 10, true)) {
        sdSuccess = true;
        Serial.printf("[SD] Mounted successfully at %lu MHz.\n", freq / 1000000UL);
        break;
      }
      Serial.printf("[SD] Mount failed at %lu MHz (attempt %d). Cleaning up & retrying...\n", freq / 1000000UL, retry);
      SD.end();
#if defined(SD_CS_PIN) && (SD_CS_PIN >= 0)
      digitalWrite(SD_CS_PIN, HIGH);
#endif
      delay(150); // Give SD card controller time to reset before next attempt
    }
    if (sdSuccess) break;
  }

  // Load settings from NVS
  int cachedTheme = 0;
  int cachedOrientation = 1;
  bool cachedWifiEnabled = false;
  {
    Preferences prefs;
    prefs.begin("settings", false);
    unsigned long loadedDelay = slideshowTimer.getInterval();
    HardwareLogic::loadSettings(prefs, currentBrightness, isAutoBrightness, loadedDelay, isRandomMode, showFilename, isInactivitySleep, currentThemeFlavor, currentOrientation, currentLedBrightness, isLedEnabled, isWifiEnabled, wifiSSID, wifiPassword, bypassOptimization);
    slideshowTimer.setInterval(loadedDelay);
    cachedTheme = (int)prefs.getUInt("cached_theme", 0);
    cachedOrientation = (int)prefs.getInt("cached_rot", 1);
    cachedWifiEnabled = prefs.getBool("cached_wifi", false);
    prefs.end();
    Serial.printf("[System] Settings loaded from NVS. Brightness: %d, Auto: %d, Delay: %lu ms, Random: %d, ShowFN: %d, Sleep: %d, Theme: %d, CachedTheme: %d, Orientation: %d, CachedOrientation: %d, LED Brightness: %d, LED Enabled: %d, WiFi: %d, CachedWiFi: %d, BypassOpt: %d\n",
                  currentBrightness, isAutoBrightness, loadedDelay, isRandomMode, showFilename, isInactivitySleep, currentThemeFlavor, cachedTheme, currentOrientation, cachedOrientation, currentLedBrightness, isLedEnabled, isWifiEnabled, cachedWifiEnabled, bypassOptimization);
  }

  lastTouchTimeMs = millis();

  // Initialize LDR Sensor Pin
  pinMode(LDR_PIN, INPUT);

  // Initialize RGB LED
  led.begin();
  led.setEnabled(isLedEnabled);
  led.setBrightness(currentLedBrightness);
  led.setState(LedManager::STATE_BOOT);

  // Initialize TFT
  tft.begin();
  tft.setRotation(currentOrientation);

  // Initialize Touch Screen
  TouchManager::begin();
  touchHandler.setOrientation(currentOrientation);

  // Initialize LVGL
  LVGLManager::init(tft.width(), tft.height());
  LVGLManager::setExitCallback(triggerExitSettings);
  LVGLManager::setClearCacheCallback(handleClearCache);

  if (!sdSuccess) {
    led.setState(LedManager::STATE_ERROR);
    showSDError(); // Blocks execution here if failed
  }
  Serial.println("SD Card mounted successfully.");

  // Initialize and begin WiFi Manager if enabled
  if (isWifiEnabled) {
    wifiManager = new WifiManager(wifiSSID, wifiPassword);
    wifiManager->begin();

    // If attempting STA connection with credentials, wait briefly to check status
    if (wifiManager->getState() == WIFI_STATE_CONNECTING) {
      Serial.println("[WiFi] Connecting to Wi-Fi network before optimization phase...");
      unsigned long wifiStart = millis();
      while (wifiManager->getState() == WIFI_STATE_CONNECTING && millis() - wifiStart < 16000) {
        wifiManager->update();
        delay(20);
      }
    }

    // If in AP Mode (no credentials or failed connection), show captive portal screen before photo calculation/optimization
    if (wifiManager->getState() == WIFI_STATE_AP_MODE) {
      Serial.println("[WiFi] Device in AP Mode. Showing Captive Portal screen before photo optimization...");
#if defined(TFT_BL) && (TFT_BL >= 0)
      pinMode(TFT_BL, OUTPUT);
      analogWrite(TFT_BL, currentBrightness);
#endif
      fader.startFade(currentBrightness, currentBrightness, 0);
      LVGLManager::showAPModeScreen(wifiManager->getAPSSID().c_str(), wifiManager->getIPAddress().c_str());

      while (wifiManager->getState() == WIFI_STATE_AP_MODE) {
        wifiManager->update();
        LVGLManager::handle();

#if defined(TFT_BL) && (TFT_BL >= 0)
        analogWrite(TFT_BL, currentBrightness);
#endif

        if (pendingExitSettings) {
          pendingExitSettings = false;
          exitSettings();
          if (wifiManager->getState() != WIFI_STATE_AP_MODE) {
            break;
          }
          tft.fillScreen(CTP_BASE);
          LVGLManager::showAPModeScreen(wifiManager->getAPSSID().c_str(), wifiManager->getIPAddress().c_str());
        }

        bool touched = TouchManager::isTouched();
        if (touched) {
          int tx = 0, ty = 0;
          if (TouchManager::getTouchPoint(tx, ty)) {
            lastTouchTimeMs = millis();
            TouchZone zone = touchHandler.processTouch(touched, tx, ty, millis());
            if (zone == TouchZone::LONG_PRESS_MID_CENTER && currentState != STATE_SETTINGS) {
              Serial.println("[Touch] Long Press Center in AP Mode - Entering Settings Menu");
              currentState = STATE_SETTINGS;
              led.setState(LedManager::STATE_SETTINGS);
              slideshowTimer.setPaused(true);
              tft.fillScreen(CTP_BASE);
              LVGLManager::hideAPModeScreen();
              LVGLManager::showSettings();
            }
          }
        }
        delay(5);
      }
      LVGLManager::hideAPModeScreen();
    }
  }

  // Keep backlight OFF initially so optimization screen renders silently without flashing leftover image
#if defined(TFT_BL) && (TFT_BL >= 0)
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, 0);
#endif

  if (!bypassOptimization) {
    // Show calculating screen in darkness first
    drawCalculating();
    LVGLManager::handle();

    // Now turn on backlight to reveal the optimization screen cleanly
#if defined(TFT_BL) && (TFT_BL >= 0)
    analogWrite(TFT_BL, currentBrightness);
#endif
  } else {
    // Bypass: show a brief loading splash so the user knows the device is booting
    tft.fillScreen(CTP_BASE);
    tft.setTextColor(CTP_TEXT, CTP_BASE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("CYD Photo Frame", tft.width() / 2, tft.height() / 2 - 20, 4);
    tft.setTextColor(CTP_GREEN, CTP_BASE);
    tft.drawString("Loading slideshow...", tft.width() / 2, tft.height() / 2 + 20, 2);
#if defined(TFT_BL) && (TFT_BL >= 0)
    analogWrite(TFT_BL, currentBrightness);
#endif
  }

  // Initialize the TJpg_Decoder
  TJpgDec.setCallback(tft_output);
  TJpgDec.setSwapBytes(true);

  // Populate cache
  Serial.println("[System] Populating file cache from SD root...");
  populateCache();
  Serial.printf("[System] File cache populated: %zu image(s) found.\n", fileCache.size());
  if (fileCache.isEmpty()) {
    Serial.println("Error: No images found on SD card.");
    led.setState(LedManager::STATE_ERROR);
    LVGLManager::showNoPhotosWarning();
    while(true) {
      LVGLManager::handle();
      delay(10);
    }
  }

  // Ensure cache directory exists
  if (!SD.exists("/cache")) {
    SD.mkdir("/cache");
  }

  // ------------------------------------------------------------------
  // Single-pass /cache sweep:
  // 1. Clears cache if the theme changed.
  // 2. Deletes leftover temporary (.tmp) files.
  // 3. Populates cacheInventory (filename -> size) with valid cache files.
  // This reduces up to three separate slow directory sweeps down to one.
  // ------------------------------------------------------------------
  std::vector<std::pair<uint64_t, size_t>> cacheInventory;
  cacheInventory.reserve(fileCache.size() + 20); // Pre-allocate to prevent mid-scan heap allocation/fragmentation

  bool themeChanged = (currentThemeFlavor != cachedTheme);
  if (themeChanged) {
    Serial.println("[System] Theme flavor changed. Clearing cache...");
  } else if (!bypassOptimization) {
    Serial.println("[System] Scanning /cache inventory...");
  }

  {
    File cacheDir = SD.open("/cache");
    if (cacheDir) {
      File file = cacheDir.openNextFile();
      while (file) {
        if (!file.isDirectory()) {
          String path = file.path();
          size_t fileSize = (size_t)file.size();
          file.close();

          bool deleted = false;
          if (themeChanged) {
            SD.remove(path.c_str());
            deleted = true;
          } else if (!bypassOptimization && path.endsWith(".tmp")) {
            Serial.printf("[System] Cleaning up orphaned temp file: %s\n", path.c_str());
            SD.remove(path.c_str());
            deleted = true;
          }

          if (!deleted && !bypassOptimization) {
            uint64_t h = fnv1a_hash(path.c_str());
            cacheInventory.push_back({h, fileSize});
          }
        } else {
          file.close();
        }
        file = cacheDir.openNextFile();
      }
      cacheDir.close();
    }
    if (!bypassOptimization && !themeChanged) {
      std::sort(cacheInventory.begin(), cacheInventory.end());
      Serial.printf("[System] Cache inventory built: %zu file(s) found in /cache.\n", cacheInventory.size());
    }
  }

  // Save new cached values in NVS if theme, orientation, or WiFi changed
  if (currentThemeFlavor != cachedTheme || currentOrientation != cachedOrientation || isWifiEnabled != cachedWifiEnabled) {
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putUInt("cached_theme", (uint32_t)currentThemeFlavor);
    prefs.putInt("cached_rot", currentOrientation);
    prefs.putBool("cached_wifi", isWifiEnabled);
    prefs.end();
    cachedTheme = currentThemeFlavor; // Sync local variable
    cachedOrientation = currentOrientation; // Sync local variable
    cachedWifiEnabled = isWifiEnabled; // Sync local variable
  }

  if (!bypassOptimization) {
    // Scan for files that need optimization/caching
    size_t totalImages = fileCache.size();
    std::vector<size_t> filesToCache;
    filesToCache.reserve(totalImages); // Pre-allocate to prevent heap allocation/fragmentation during calculations

    unsigned long lastTouchCheckMs = 0;
    size_t expectedCacheBytes = (size_t)(tft.width() * tft.height() * 2);

    Serial.printf("[System] Beginning SD Card image calculation phase for %zu total image(s) (%dx%d resolution, expected cache size: %zu bytes)...\n",
                  totalImages, tft.width(), tft.height(), expectedCacheBytes);

    for (size_t i = 0; i < totalImages; i++) {
      std::string originalPath = fileCache.getAt(i);
      std::string cachePath = getCachePath(originalPath);

      const char* displayName = strrchr(originalPath.c_str(), '/');
      displayName = displayName ? displayName + 1 : originalPath.c_str();

      // Update screen labels & progress bar dynamically
      LVGLManager::updateCalculationProgress(i + 1, totalImages, displayName, filesToCache.size());

      // Poll touch screen at most once every 50ms to prevent XPT2046 bus lockup
      unsigned long loopNow = millis();
      if (loopNow - lastTouchCheckMs >= 50) {
        lastTouchCheckMs = loopNow;
        LVGLManager::handle();
        if (optimizationCancelled) {
          Serial.println("[System] Optimization cancelled during calculation (via LVGL callback).");
          drawCancellingFeedback();
          restrictCacheToExisting();
          filesToCache.clear();
          break;
        }
        if (TouchManager::isTouched()) {
          int tx = 0, ty = 0;
          if (TouchManager::getTouchPoint(tx, ty)) {
            Serial.printf("[Touch Debug] Touch detected during calculation: Raw X=%d, Y=%d\n", tx, ty);
            if (isCancelButtonTouched(tx, ty)) {
              Serial.println("[System] Optimization CANCELLED by user touch during calculation phase!");
              optimizationCancelled = true;
              drawCancellingFeedback();
              restrictCacheToExisting();
              filesToCache.clear();
              break;
            }
          }
        }
      }

      // Look up this image in the pre-built inventory (zero SD I/O).
      bool cacheValid = false;
      size_t actualSize = 0;
      uint64_t h = fnv1a_hash(cachePath.c_str());

      auto it = std::lower_bound(cacheInventory.begin(), cacheInventory.end(), std::make_pair(h, (size_t)0),
                                 [](const std::pair<uint64_t, size_t>& a, const std::pair<uint64_t, size_t>& b) {
                                   return a.first < b.first;
                                 });

      if (it != cacheInventory.end() && it->first == h) {
        actualSize = it->second;
        if (actualSize == expectedCacheBytes) {
          cacheValid = true;
        } else {
          // Stale/wrong-size cache file — remove it from SD.
          Serial.printf("[System] [Calculation %zu/%zu] '%s': Stale/invalid cache size (%zu B vs expected %zu B). Removing...\n",
                        i + 1, totalImages, displayName, actualSize, expectedCacheBytes);
          SD.remove(cachePath.c_str());
        }
      }

      if (!cacheValid) {
        filesToCache.push_back(i);
        Serial.printf("[System] [Calculation %zu/%zu] '%s' -> Cache MISS (Needs optimization) [Total needing opt: %zu]\n",
                      i + 1, totalImages, displayName, filesToCache.size());
      } else {
        Serial.printf("[System] [Calculation %zu/%zu] '%s' -> Cache HIT (Valid %zu B raw file present)\n",
                      i + 1, totalImages, displayName, actualSize);
      }
    }

    Serial.printf("[System] Calculation phase complete! Scanned %zu images | %zu cached | %zu requiring optimization.\n",
                  totalImages, totalImages - filesToCache.size(), filesToCache.size());

    // If there are files to cache, display progress bar and generate them silently
    if (!filesToCache.empty()) {
      Serial.printf("[System] Starting batch optimization phase for %zu photo(s)...\n", filesToCache.size());
#if defined(TFT_BL) && (TFT_BL >= 0)
      // Make sure backlight is on during progress bar display
      analogWrite(TFT_BL, currentBrightness);
#endif
      
      optimizationCancelled = false; // Reset before starting optimization loop
      led.setState(LedManager::STATE_OPTIMIZING);
      bool cancelled = false;
      unsigned long lastOptTouchCheckMs = 0;
      for (size_t i = 0; i < filesToCache.size(); i++) {
        size_t fileIdx = filesToCache[i];
        std::string originalPath = fileCache.getAt(fileIdx);
        std::string cachePath = getCachePath(originalPath);

        const char* displayName = strrchr(originalPath.c_str(), '/');
        displayName = displayName ? displayName + 1 : originalPath.c_str();

        // Update progress bar & labels
        drawProgress(i, filesToCache.size(), displayName);
        
        // Poll touch screen between files (rate-limited to 50ms)
        unsigned long optNow = millis();
        if (optNow - lastOptTouchCheckMs >= 50) {
          lastOptTouchCheckMs = optNow;
          LVGLManager::handle();
          if (optimizationCancelled) {
            Serial.println("[System] Optimization cancelled during optimization loop (via LVGL callback).");
            cancelled = true;
            drawCancellingFeedback();
            break;
          }
          if (TouchManager::isTouched()) {
            int tx = 0, ty = 0;
            if (TouchManager::getTouchPoint(tx, ty)) {
              Serial.printf("[Touch Debug] Touch detected during optimization loop: Raw X=%d, Y=%d\n", tx, ty);
              if (isCancelButtonTouched(tx, ty)) {
                Serial.println("[System] Optimization CANCELLED by user touch during optimization phase!");
                cancelled = true;
                drawCancellingFeedback();
                break;
              }
            }
          }
        }

        if (optimizationCancelled) {
          cancelled = true;
          break;
        }
        
        Serial.printf("[System] [Optimization %zu/%zu] Resizing & caching '%s' -> '%s'...\n",
                      i + 1, filesToCache.size(), displayName, cachePath.c_str());

        unsigned long optStart = millis();
        generateCache(originalPath.c_str(), cachePath.c_str());
        unsigned long optElapsed = millis() - optStart;

        Serial.printf("[System] [Optimization %zu/%zu] Completed '%s' in %lu ms.\n",
                      i + 1, filesToCache.size(), displayName, optElapsed);

        if (optimizationCancelled) {
          cancelled = true;
          break;
        }
      }
      
      if (cancelled) {
        restrictCacheToExisting();
        
        tft.fillScreen(CTP_BASE);
        tft.setTextColor(CTP_TEXT, CTP_BASE);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("CYD Photo Frame", tft.width() / 2, tft.height() / 2 - 20, 4);
        tft.setTextColor(CTP_RED, CTP_BASE);
        tft.drawString("Optimization cancelled. Loading slideshow...", tft.width() / 2, tft.height() / 2 + 20, 2);
        delay(1500);
      } else {
        drawProgress(filesToCache.size(), filesToCache.size(), "All Photos Optimized!");
        delay(1500);
      }
    } else {
      // If all photos were already cached, display completion status on screen for 1.5s
      LVGLManager::updateCalculationProgress(totalImages, totalImages, "All Photos Ready!", 0);
      delay(1500);
    }

    // Smoothly fade out optimization / progress screen to black before loading first image
    fader.startFade(currentBrightness, 0, 250);
    while (fader.getState() != BacklightFader::STATE_IDLE) {
      int b = 0;
      fader.update(millis(), b);
#if defined(TFT_BL) && (TFT_BL >= 0)
      analogWrite(TFT_BL, b);
#endif
      delay(10);
    }
    // Ensure backlight is off before rendering first image in darkness
#if defined(TFT_BL) && (TFT_BL >= 0)
    analogWrite(TFT_BL, 0);
#endif
  }

  LVGLManager::hideOptimizationScreen();
  tft.fillScreen(CTP_BASE);

  // Fade out loading/optimization screen to black before rendering first image
  if (bypassOptimization) {
    fader.startFade(currentBrightness, 0, 250);
    while (fader.getState() != BacklightFader::STATE_IDLE) {
      int b = 0;
      fader.update(millis(), b);
#if defined(TFT_BL) && (TFT_BL >= 0)
      analogWrite(TFT_BL, b);
#endif
      delay(10);
    }
#if defined(TFT_BL) && (TFT_BL >= 0)
    analogWrite(TFT_BL, 0);
#endif
  }

  // Pause briefly in darkness for a smooth transition before loading the first photo
  delay(300);

  Serial.printf("[System] Rendering first image (bypass=%d, cache=%zu)...\n", bypassOptimization, fileCache.size());
  // Find and render the first valid image in complete darkness (no line scan or flashing!)
  bool success = false;
  size_t attempts = 0;
  size_t maxAttempts = fileCache.size();
  while (!success && attempts < maxAttempts) {
    success = renderScaledJpg(fileCache.getCurrent().c_str());
    if (!success) {
      Serial.printf("[System] First image failed to render. Skipping...\n");
      fileCache.getNext();
      attempts++;
    }
  }

  if (!success) {
    Serial.println("Error: All images in cache failed to render on startup.");
    led.setState(LedManager::STATE_ERROR);
    tft.fillScreen(CTP_BASE);
    tft.setTextColor(CTP_RED, CTP_BASE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("All Images Failed", tft.width() / 2, tft.height() / 2, 4);
    while(true) { led.update(millis()); delay(100); } // Halt, keep LED updating
  }

  led.setState(LedManager::STATE_SLIDESHOW);
  slideshowTimer.start(millis());

  // Smoothly fade in from black to display the first image
  transitionPending = true;
  fader.startFade(0, currentBrightness, 300);
}

std::string getNextImageFile() {
  if (isRandomMode && fileCache.size() > 0) {
    size_t randIdx = random(fileCache.size());
    fileCache.setIndex(randIdx);
    return fileCache.getCurrent();
  }
  return fileCache.getNext();
}

/**
 * @brief Attempts to display the next image, skipping any corrupt/invalid ones.
 */
void showNextImage() {
  bool success = false;
  size_t attempts = 0;
  size_t maxAttempts = fileCache.size();
  while (!success && attempts < maxAttempts) {
    std::string nextFile = getNextImageFile();
    success = renderScaledJpg(nextFile.c_str());
    if (!success) {
      Serial.printf("[System] Image failed to render. Skipping to next...\n");
      attempts++;
    }
  }
  if (!success) {
    tft.fillScreen(CTP_BASE);
    tft.setTextColor(CTP_RED, CTP_BASE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("All Images Failed", tft.width() / 2, tft.height() / 2, 4);
  }
}

/**
 * @brief Attempts to display the previous image, skipping any corrupt/invalid ones.
 */
void showPreviousImage() {
  bool success = false;
  size_t attempts = 0;
  size_t maxAttempts = fileCache.size();
  while (!success && attempts < maxAttempts) {
    std::string prevFile = fileCache.getPrevious();
    success = renderScaledJpg(prevFile.c_str());
    if (!success) {
      Serial.printf("[System] Image failed to render. Skipping to previous...\n");
      attempts++;
    }
  }
  if (!success) {
    tft.fillScreen(CTP_BASE);
    tft.setTextColor(CTP_RED, CTP_BASE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("All Images Failed", tft.width() / 2, tft.height() / 2, 4);
  }
}

void loop() {
  if (wifiManager != nullptr) {
    wifiManager->update();
  }

  if (pendingExitSettings) {
    pendingExitSettings = false;
    exitSettings();
  }

  // Handle AP Mode configuration screen display
  static bool apScreenShown = false;
  if (wifiManager != nullptr && wifiManager->getState() == WIFI_STATE_AP_MODE && currentState != STATE_SETTINGS) {
    if (!apScreenShown) {
      apScreenShown = true;
      LVGLManager::showAPModeScreen(wifiManager->getAPSSID().c_str(), wifiManager->getIPAddress().c_str());
#if defined(TFT_BL) && (TFT_BL >= 0)
      analogWrite(TFT_BL, currentBrightness);
#endif
      fader.startFade(currentBrightness, currentBrightness, 0);
    }

    LVGLManager::handle();

#if defined(TFT_BL) && (TFT_BL >= 0)
    analogWrite(TFT_BL, currentBrightness);
#endif

    // Check for touch input to go to settings in AP Mode
    bool touched = TouchManager::isTouched();
    int rawX = 0, rawY = 0;
    if (touched) {
      TouchManager::getTouchPoint(rawX, rawY);
      lastTouchTimeMs = millis();
      TouchZone zone = touchHandler.processTouch(touched, rawX, rawY, millis());
      if (zone == TouchZone::LONG_PRESS_MID_CENTER) {
        Serial.println("[Touch] Long Press Center in AP Mode - Entering Settings Menu");
        currentState = STATE_SETTINGS;
        led.setState(LedManager::STATE_SETTINGS);
        slideshowTimer.setPaused(true);
        tft.fillScreen(CTP_BASE);
        LVGLManager::hideAPModeScreen();
        apScreenShown = false;
        LVGLManager::showSettings();
      }
    }

    delay(5);
    return;
  } else {
    if (apScreenShown) {
      LVGLManager::hideAPModeScreen();
      apScreenShown = false;
    }
  }

  // Check for serial commands
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "clear_cache" || cmd == "clear") {
      Serial.println("[Serial] Clearing cache directory...");
      File cacheDir = SD.open("/cache");
      if (cacheDir) {
        File file = cacheDir.openNextFile();
        while (file) {
          String path = file.path();
          file.close();
          SD.remove(path.c_str());
          file = cacheDir.openNextFile();
        }
        cacheDir.close();
        Serial.println("[Serial] Cache cleared successfully! Rebooting...");
        delay(500);
#if !defined(NATIVE_TEST)
        ESP.restart();
#endif
      } else {
        Serial.println("[Serial] Error: Could not open /cache directory.");
      }
    } else if (cmd == "screenshot") {
      // Capture the current LVGL screen (settings screen only)
      if (currentState == STATE_SETTINGS) {
        std::string filename = ScreenshotManager::generateFilename(millis());
        Serial.printf("[Serial] Capturing LVGL screenshot to %s...\n", filename.c_str());
        led.setState(LedManager::STATE_SCREENSHOT);
        ScreenshotManager::captureToSD(filename.c_str());
        // Restore settings LED state after capture
        led.setState(LedManager::STATE_SETTINGS);
      } else {
        Serial.println("[Serial] 'screenshot' only works while the Settings screen is open.");
        Serial.println("[Serial] Use 'screenshot_tft' to capture the current raw TFT screen.");
      }
    } else if (cmd == "screenshot_tft") {
      // Capture the current raw TFT screen (optimization, slideshow, etc.)
      std::string filename = ScreenshotManager::generateFilename(millis());
      Serial.printf("[Serial] Capturing TFT screen to %s...\n", filename.c_str());
      LedManager::State prevLedState = (currentState == STATE_SETTINGS)
          ? LedManager::STATE_SETTINGS
          : LedManager::STATE_SLIDESHOW;
      led.setState(LedManager::STATE_SCREENSHOT);
      ScreenshotManager::captureTFTToSD(filename.c_str());
      // Restore state after capture
      led.setState(prevLedState);
    }
  }

  if (fileCache.isEmpty()) {
    delay(1000);
    return;
  }

  unsigned long now = millis();

  // Read LDR for auto-brightness and darkness sleep
  int rawLdr = analogRead(34);
  
  // Track absolute darkness sleep (LDR < 100)
  static unsigned long darknessStartTimeMs = 0;
  bool isDark = (rawLdr < 100);
  
  if (isInactivitySleep && isDark) {
    if (darknessStartTimeMs == 0) {
      darknessStartTimeMs = now;
    } else if (now - darknessStartTimeMs >= 300000UL && !isSleeping) { // 5 minutes of absolute darkness
      isSleeping = true;
      Serial.println("[System] Entering darkness sleep.");
    }
  } else {
    darknessStartTimeMs = 0;
    // Auto-wake from darkness sleep if light returns
    if (isSleeping && rawLdr >= 150) { // Hysteresis threshold
      isSleeping = false;
      led.setState(LedManager::STATE_SLIDESHOW);
      lastTouchTimeMs = now;
      Serial.println("[System] Light detected - Auto-waking from darkness sleep.");
    }
  }

  // Auto Brightness LDR Polling (GPIO 34)
  if (isAutoBrightness && !isSleeping && !transitionPending) {
    static unsigned long lastLdrReadTime = 0;
    if (now - lastLdrReadTime >= 200) { // Poll every 200ms
      int targetBrightness = HardwareLogic::mapLdrToBrightness(rawLdr);
      currentBrightness = HardwareLogic::smoothBrightness(currentBrightness, targetBrightness, 0.05f);
      lastLdrReadTime = now;
    }
  }

  // Check for touch input
  bool touched = TouchManager::isTouched();
  int rawX = 0, rawY = 0;
  if (touched) {
    TouchManager::getTouchPoint(rawX, rawY);
    lastTouchTimeMs = now; // Reset inactivity timer on physical touch
    
    static unsigned long lastTouchPrintTime = 0;
    if (now - lastTouchPrintTime > 250) { // Limit print frequency to 250ms
      Serial.printf("[Touch] Raw Touch: X=%d, Y=%d\n", rawX, rawY);
      lastTouchPrintTime = now;
    }

    // Wake screen if it was sleeping
    if (isSleeping) {
      isSleeping = false;
      led.setState(LedManager::STATE_SLIDESHOW);
      Serial.println("[System] Screen woke up on touch.");
      // Clear touchHandler state and skip processing this touch to prevent accidental navigation
      touchHandler.processTouch(false, 0, 0, now);
      delay(200); // Debounce physical touch release
      return;     // Skip processing tap zones for this click
    }
  }

  // Inactivity Sleep (if enabled)
  if (isInactivitySleep && !isSleeping) {
    if (now - lastTouchTimeMs >= 300000UL) { // 5 minutes
      isSleeping = true;
      led.setState(LedManager::STATE_OFF);
      Serial.println("[System] Entering inactivity sleep.");
    }
  }

  TouchZone zone = touchHandler.processTouch(touched, rawX, rawY, now);
  if (currentState == STATE_SLIDESHOW) {
    if (zone != TouchZone::NONE) {
      if (zone == TouchZone::LONG_PRESS_MID_CENTER) {
        Serial.println("[Touch] Long Press Center - Entering Settings Menu");
        showToastBanner("Entering Settings...", 1000);
        currentState = STATE_SETTINGS;
        led.setState(LedManager::STATE_SETTINGS);
        slideshowTimer.setPaused(true);
        tft.fillScreen(CTP_BASE);
        LVGLManager::showSettings();
      } else if (zone == TouchZone::MID_LEFT) {
        Serial.println("[Touch] Mid-Left tapped - Previous Image");
        if (!transitionPending) {
          showToastBanner("Previous Image", 1500);
          transitionPending = true;
          pendingDirection = DIR_PREV;
          fader.startFade(currentBrightness, 0, 300);
          slideshowTimer.setPaused(true);
        }
      } else if (zone == TouchZone::MID_RIGHT) {
        Serial.println("[Touch] Mid-Right tapped - Next Image");
        if (!transitionPending) {
          showToastBanner("Next Image", 1500);
          transitionPending = true;
          pendingDirection = DIR_NEXT;
          fader.startFade(currentBrightness, 0, 300);
          slideshowTimer.setPaused(true);
        }
      } else if (zone == TouchZone::MID_CENTER) {
        bool isPaused = !slideshowTimer.isPaused();
        slideshowTimer.setPaused(isPaused);
        Serial.printf("[Touch] Mid-Center tapped - %s Slideshow\n", isPaused ? "Paused" : "Resumed");
        showToastBanner(isPaused ? "Slideshow Paused" : "Slideshow Resumed", 2000);
        if (!isPaused) {
          slideshowTimer.reset(now);
        }
      } else if (zone == TouchZone::TOP_LEFT) {
        currentBrightness = min(currentBrightness + 25, 255);
        Serial.printf("[Touch] Top-Left tapped - Brightness increased to %d\n", currentBrightness);
        saveConfig();
        char msgBuf[32];
        snprintf(msgBuf, sizeof(msgBuf), "Brightness: %d%%", (currentBrightness * 100 + 127) / 255);
        showToastBanner(msgBuf, 2000);
      } else if (zone == TouchZone::BOTTOM_LEFT) {
        currentBrightness = max(currentBrightness - 25, 25);
        Serial.printf("[Touch] Bottom-Left tapped - Brightness decreased to %d\n", currentBrightness);
        saveConfig();
        char msgBuf[32];
        snprintf(msgBuf, sizeof(msgBuf), "Brightness: %d%%", (currentBrightness * 100 + 127) / 255);
        showToastBanner(msgBuf, 2000);
      } else if (zone == TouchZone::TOP_CENTER) {
        showFilename = !showFilename;
        Serial.printf("[Touch] Top-Center tapped - Filename display: %s\n", showFilename ? "ON" : "OFF");
        saveConfig();
        char msgBuf[32];
        snprintf(msgBuf, sizeof(msgBuf), "Filename Banner: %s", showFilename ? "ON" : "OFF");
        showToastBanner(msgBuf, 2000);
        renderScaledJpg(fileCache.getCurrent().c_str());
      } else if (zone == TouchZone::BOTTOM_CENTER) {
        isRandomMode = !isRandomMode;
        Serial.printf("[Touch] Bottom-Center tapped - Random mode: %s\n", isRandomMode ? "ON" : "OFF");
        saveConfig();
        char msgBuf[32];
        snprintf(msgBuf, sizeof(msgBuf), "Random Mode: %s", isRandomMode ? "ON" : "OFF");
        showToastBanner(msgBuf, 2000);
      } else if (zone == TouchZone::TOP_RIGHT) {
        unsigned long currentDelay = slideshowTimer.getInterval();
        currentDelay = min(currentDelay + 1000, 60000UL);
        slideshowTimer.setInterval(currentDelay);
        Serial.printf("[Touch] Top-Right tapped - Delay increased to %lu ms\n", currentDelay);
        saveConfig();
        char msgBuf[32];
        snprintf(msgBuf, sizeof(msgBuf), "Slideshow Delay: %lus", currentDelay / 1000UL);
        showToastBanner(msgBuf, 2000);
      } else if (zone == TouchZone::BOTTOM_RIGHT) {
        unsigned long currentDelay = slideshowTimer.getInterval();
        currentDelay = max(currentDelay - 1000, 2000UL);
        slideshowTimer.setInterval(currentDelay);
        Serial.printf("[Touch] Bottom-Right tapped - Delay decreased to %lu ms\n", currentDelay);
        saveConfig();
        char msgBuf[32];
        snprintf(msgBuf, sizeof(msgBuf), "Slideshow Delay: %lus", currentDelay / 1000UL);
        showToastBanner(msgBuf, 2000);
      }
    }

    if (slideshowTimer.isElapsed(now) && !transitionPending) {
      transitionPending = true;
      pendingDirection = DIR_NEXT;
      fader.startFade(currentBrightness, 0, 300);
      slideshowTimer.setPaused(true);
    }
  } else {
    // In STATE_SETTINGS, clear touchHandler's state machine
    touchHandler.processTouch(false, 0, 0, now);
  }

  if (toastActive && now >= toastEndTimeMs) {
    clearToastBanner();
  }

  // Tick the fader and obtain current interpolated brightness
  int fadeBrightness = currentBrightness;
  bool fadeDone = fader.update(now, fadeBrightness);

  if (transitionPending && fader.getState() == BacklightFader::STATE_IDLE && fadeDone) {
    if (fadeBrightness == 0) {
      // Fade out complete: turn off backlight pin immediately before rendering new image
#if defined(TFT_BL) && (TFT_BL >= 0)
      analogWrite(TFT_BL, 0);
#endif
      // Render next image in total darkness (no line scan or tearing visible!)
      if (pendingDirection == DIR_NEXT) {
        showNextImage();
      } else {
        showPreviousImage();
      }
      led.setState(LedManager::STATE_TRANSITION);
      // Start fading back in to target brightness
      fader.startFade(0, currentBrightness, 300);
    } else {
      // Fade in complete
      transitionPending = false;
      slideshowTimer.setPaused(false);
      slideshowTimer.reset(now);
    }
  }

  // Apply final brightness to backlight pin
  int finalBacklight = isSleeping ? 0 : fadeBrightness;
#if defined(TFT_BL) && (TFT_BL >= 0)
  analogWrite(TFT_BL, finalBacklight);
#endif

  if (currentState == STATE_SETTINGS) {
    LVGLManager::handle();
  }

  // Update LED state machine every loop tick
  led.update(millis());

  // Use high-framerate (10ms) loop delay during backlight transitions for smooth fading
  int loopDelayMs = (transitionPending || fader.getState() != BacklightFader::STATE_IDLE) ? 10 : 50;
  delay(loopDelayMs);
}
