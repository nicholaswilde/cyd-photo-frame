#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include "file_cache.h"
#include "slideshow_timer.h"
#include "touch_manager.h"
#include "touch_handler.h"
#include "lvgl_manager.h"
#include "hardware_logic.h"
#include "backlight_fader.h"

BacklightFader fader;
bool transitionPending = false;
enum TransitionDirection { DIR_NEXT, DIR_PREV };
TransitionDirection pendingDirection = DIR_NEXT;

// Initialize the TFT object. 
// Note: Pins and drivers are automatically handled by platformio.ini build_flags!
TFT_eSPI tft = TFT_eSPI();
FileCache fileCache(1024);
SlideshowTimer slideshowTimer(10000);
TouchHandler touchHandler(TFT_HEIGHT, TFT_WIDTH);

int currentBrightness = 255;
bool showFilename = true;
bool isRandomMode = false;
bool isAutoBrightness = false;
bool isInactivitySleep = false;
bool optimizationCancelled = false;

bool isCancelButtonTouched(int touchX, int touchY);

#include "catppuccin.h"

int currentThemeFlavor = CATPPUCCIN_MOCHA;

#include "app_state.h"
AppState currentState = STATE_SLIDESHOW;

bool isSleeping = false;
unsigned long lastTouchTimeMs = 0;

float current_scale_sw = 1.0f;
int16_t current_x_offset = 0;
int16_t current_y_offset = 0;

// SD Card Chip Select for CYD
// const uint8_t SD_CS_PIN = 5;

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
  return "/cache/" + base + ".raw";
}

bool renderRawImage(const char* filename) {
  File rawFile = SD.open(filename, FILE_READ);
  if (!rawFile) {
    Serial.printf("Failed to open raw image: %s\n", filename);
    return false;
  }
  
  // Allocate buffer on the heap to avoid stack overflow
  const size_t bufferSize = 320 * 16;
  uint16_t* buffer = (uint16_t*)malloc(bufferSize * sizeof(uint16_t));
  if (!buffer) {
    Serial.println("Failed to allocate buffer for raw image rendering!");
    rawFile.close();
    return false;
  }
  
  tft.startWrite();
  tft.setAddrWindow(0, 0, 320, 240);
  
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

/**
 * @brief Callback function required by TJpg_Decoder.
 * Pushes decompressed MCU blocks (usually 16x16 pixels) to the screen.
 */
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  // Poll touch screen periodically during decoding (e.g., every 20 blocks) to check for Cancel
  static int blockCounter = 0;
  blockCounter++;
  if (blockCounter >= 20) {
    blockCounter = 0;
    if (TouchManager::isTouched()) {
      int tx = 0, ty = 0;
      if (TouchManager::getTouchPoint(tx, ty)) {
        Serial.printf("[Touch Debug] Touch detected during decode at X=%d, Y=%d\n", tx, ty);
        if (isCancelButtonTouched(tx, ty)) {
          Serial.println("[System] Optimization cancelled during file write.");
          optimizationCancelled = true;
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

  if (dest_y >= 240) return 0;
  
  int16_t w_visible = w_scaled;
  if (dest_x >= 320) return 1;
  if (dest_x + w_visible > 320) {
    w_visible = 320 - dest_x;
  }
  
  int16_t h_visible = h_scaled;
  if (dest_y + h_visible > 240) {
    h_visible = 240 - dest_y;
  }

  // Bypass software scaling completely if scale is 1
  if (current_scale_sw == 1.0f) {
    if (!cacheFileActive) {
      tft.pushImage(dest_x, dest_y, w_visible, h_visible, bitmap, w);
    } else {
      if (cacheFile) {
        for (int row = 0; row < h_visible; row++) {
          unsigned long fileOffset = ((dest_y + row) * 320 + dest_x) * 2;
          cacheFile.seek(fileOffset);
          cacheFile.write((uint8_t*)(bitmap + row * w), w_visible * 2);
        }
      }
    }
    return 1;
  }

  // Software scale is > 1. Copy and downsample to scaled_bitmap.
  // Maximum possible scaled block size is (16/1)*(16/1) = 256 pixels.
  uint16_t scaled_bitmap[256];
  
  // Safe bounds guard
  uint16_t safe_w_scaled = w_scaled > 16 ? 16 : w_scaled;
  uint16_t safe_h_scaled = h_scaled > 16 ? 16 : h_scaled;

  for (uint16_t row = 0; row < safe_h_scaled; row++) {
    int16_t src_y = (int16_t)floor(((float)(y_start_scaled + row) + 0.5f) * current_scale_sw) - y;
    if (src_y < 0) src_y = 0;
    if (src_y >= h) src_y = h - 1;
    
    for (uint16_t col = 0; col < safe_w_scaled; col++) {
      int16_t src_x = (int16_t)floor(((float)(x_start_scaled + col) + 0.5f) * current_scale_sw) - x;
      if (src_x < 0) src_x = 0;
      if (src_x >= w) src_x = w - 1;
      
      scaled_bitmap[row * safe_w_scaled + col] = bitmap[src_y * w + src_x];
    }
  }

  if (!cacheFileActive) {
    tft.pushImage(dest_x, dest_y, w_visible, h_visible, scaled_bitmap, safe_w_scaled);
  } else {
    if (cacheFile) {
      for (int row = 0; row < h_visible; row++) {
        unsigned long fileOffset = ((dest_y + row) * 320 + dest_x) * 2;
        cacheFile.seek(fileOffset);
        uint16_t* src_ptr = scaled_bitmap + row * safe_w_scaled;
        cacheFile.write((uint8_t*)src_ptr, w_visible * 2);
      }
    }
  }
  return 1;
}

bool generateCache(const char* jpgFilename, const char* rawFilename) {
  Serial.printf("[System] Optimizing: %s\n", jpgFilename);
  std::string tempFilename = std::string(rawFilename) + ".tmp";
  if (SD.exists(tempFilename.c_str())) {
    SD.remove(tempFilename.c_str()); // Clean start
  }
  cacheFile = SD.open(tempFilename.c_str(), FILE_WRITE);
  if (!cacheFile) {
    Serial.printf("Failed to create cache file: %s\n", tempFilename.c_str());
    return false;
  }

  // Pre-allocate and fill with background color using a dynamic heap buffer to prevent stack overflow
  const size_t chunkSize = 320 * 16;
  uint16_t* chunk = (uint16_t*)malloc(chunkSize * sizeof(uint16_t));
  if (chunk) {
    uint16_t swapped_base = (CTP_BASE >> 8) | (CTP_BASE << 8);
    for (size_t i = 0; i < chunkSize; i++) {
      chunk[i] = swapped_base;
    }
    for (int i = 0; i < 240; i += 16) {
      cacheFile.write((uint8_t*)chunk, chunkSize * sizeof(uint16_t));
    }
    free(chunk);
  } else {
    // Stack fallback using small 320 element row buffer (only 640 bytes, stack-safe)
    uint16_t rowBuffer[320];
    uint16_t swapped_base = (CTP_BASE >> 8) | (CTP_BASE << 8);
    for (int i = 0; i < 320; i++) {
      rowBuffer[i] = swapped_base;
    }
    for (int i = 0; i < 240; i++) {
      cacheFile.write((uint8_t*)rowBuffer, 320 * sizeof(uint16_t));
    }
  }

  uint16_t img_w = 0, img_h = 0;
  uint16_t result = TJpgDec.getSdJpgSize(&img_w, &img_h, jpgFilename);
  if (result != 0) {
    Serial.printf("Failed to read header for caching: %s\n", jpgFilename);
    cacheFile.close();
    if (SD.exists(tempFilename.c_str())) {
      SD.remove(tempFilename.c_str());
    }
    return false;
  }

  float ratio_w = (float)img_w / 320.0f;
  float ratio_h = (float)img_h / 240.0f;
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

  if (max_ratio > (float)scale_hw) {
    scale_sw = max_ratio / (float)scale_hw;
  } else {
    scale_sw = 1.0f;
  }

  TJpgDec.setJpgScale(scale_hw);

  int16_t scaled_w = (int16_t)round((float)img_w / ((float)scale_hw * scale_sw));
  int16_t scaled_h = (int16_t)round((float)img_h / ((float)scale_hw * scale_sw));
  int16_t x_offset = (320 - scaled_w) / 2;
  int16_t y_offset = (240 - scaled_h) / 2;

  current_scale_sw = scale_sw;
  current_x_offset = x_offset;
  current_y_offset = y_offset;

  cacheFileActive = true;
  uint16_t drawResult = TJpgDec.drawSdJpg(0, 0, jpgFilename);
  cacheFileActive = false;

  cacheFile.close();

  if (drawResult != 0 && drawResult != 1) {
    Serial.printf("Error during caching decode: %d\n", drawResult);
    if (SD.exists(tempFilename.c_str())) {
      SD.remove(tempFilename.c_str());
    }
    return false;
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

bool isCancelButtonTouched(int rawX, int rawY) {
  int pixelX = 0;
  int pixelY = 0;
  bool isCapacitive = (rawX <= tft.width() * 2 && rawY <= tft.height() * 2);
  if (isCapacitive) {
    pixelX = rawX;
    pixelY = rawY;
  } else {
    pixelX = mapRangeClippedMain(rawX, 200, 3800, tft.width());
    pixelY = mapRangeClippedMain(rawY, 200, 3800, tft.height());
  }

  Serial.printf("[Touch Debug] Mapped Coordinates: PixelX=%d, PixelY=%d\n", pixelX, pixelY);

  int btnW = 100;
  int btnH = 30;
  int btnX = (tft.width() - btnW) / 2;
  int btnY = tft.height() - 40;
  
  return (pixelX >= btnX && pixelX <= (btnX + btnW) &&
          pixelY >= btnY && pixelY <= (btnY + btnH));
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
        if (checkFile.size() == 153600) {
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
  tft.fillScreen(CTP_BASE);

  tft.setTextColor(CTP_TEXT, CTP_BASE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("CYD Photo Frame", tft.width() / 2, 40, 4);

  tft.setTextColor(CTP_TEXT, CTP_BASE);
  tft.drawString("Optimizing Photos...", tft.width() / 2, 75, 2);

  tft.setTextColor(0x7BEF, CTP_BASE);
  tft.drawString("Analyzing SD Card...", tft.width() / 2, 105, 2);

  tft.setTextColor(CTP_TEXT, CTP_BASE);
  tft.drawString("Calculating...", tft.width() / 2, 175, 2);

  int barX = 40;
  int barY = 130;
  int barW = 240;
  int barH = 20;

  tft.fillRect(barX, barY, barW, barH, CTP_SURFACE0);
  drawCancelButton();
}

void drawProgress(size_t current, size_t total, const char* filename) {
  if (current == 0) {
    tft.fillScreen(CTP_BASE);
  }

  tft.setTextColor(CTP_TEXT, CTP_BASE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("CYD Photo Frame", tft.width() / 2, 40, 4);

  tft.setTextColor(CTP_TEXT, CTP_BASE);
  tft.drawString("Optimizing Photos...", tft.width() / 2, 75, 2);

  // Clear the filename area to prevent text overlap
  tft.fillRect(0, 95, tft.width(), 20, CTP_BASE);
  tft.setTextColor(0x7BEF, CTP_BASE);
  tft.drawString(filename, tft.width() / 2, 105, 2);

  int percentage = (total == 0) ? 0 : (current * 100) / total;
  char percentStr[32];
  sprintf(percentStr, "%d%% (%zu/%zu)", percentage, current, total);
  
  // Clear the percentage text area to prevent text overlap
  tft.fillRect(0, 165, tft.width(), 20, CTP_BASE);
  tft.setTextColor(CTP_TEXT, CTP_BASE);
  tft.drawString(percentStr, tft.width() / 2, 175, 2);

  int barX = 40;
  int barY = 130;
  int barW = 240;
  int barH = 20;
  int border = 2;

  tft.fillRect(barX, barY, barW, barH, CTP_SURFACE0);
  
  int fillW = (total == 0) ? 0 : (current * barW) / total;
  if (fillW > 0) {
    tft.fillRect(barX + border, barY + border, fillW - 2 * border, barH - 2 * border, CTP_GREEN);
  }
  drawCancelButton();
}

/**
 * @brief Displays an error state if the SD card is missing or unreadable.
 */
void showSDError() {
  tft.fillScreen(CTP_BASE);
  
  tft.setTextColor(CTP_RED, CTP_BASE);
  tft.setTextDatum(MC_DATUM); // Middle center
  tft.drawString("NO SD CARD", tft.width() / 2, (tft.height() / 2) - 20, 4);
  
  tft.setTextColor(CTP_TEXT, CTP_BASE);
  tft.drawString("Insert card and reboot", tft.width() / 2, (tft.height() / 2) + 20, 2);
  
  Serial.println("Error: SD Card Mount Failed.");
  // Halt execution
  while(true) {
    delay(1000);
  }
}

void drawFilenameBanner(const char* filename) {
  const char* namePtr = strrchr(filename, '/');
  const char* displayName = namePtr ? namePtr + 1 : filename;
  
  // Draw solid Catppuccin Mantle background banner
  tft.fillRect(0, 240 - 24, 320, 24, CTP_MANTLE);
  
  // Draw text centered in the banner
  tft.setTextColor(CTP_TEXT, CTP_MANTLE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(displayName, 160, 228, 2);
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
      if (checkFile.size() == 153600) {
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

    if (max_ratio > (float)scale_hw) {
      scale_sw = max_ratio / (float)scale_hw;
    } else {
      scale_sw = 1.0f;
    }

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
  HardwareLogic::saveSettings(prefs, currentBrightness, isAutoBrightness, slideshowTimer.getInterval(), isRandomMode, showFilename, isInactivitySleep, currentThemeFlavor);
  prefs.end();
  Serial.println("[System] Settings saved to NVS.");
}

bool pendingExitSettings = false;
void triggerExitSettings() {
  pendingExitSettings = true;
}

void exitSettings() {
  int cachedTheme = 0;
  {
    Preferences prefs;
    prefs.begin("settings", false);
    cachedTheme = (int)prefs.getUInt("cached_theme", 0);
    prefs.end();
  }

  saveConfig();

#if !defined(NATIVE_TEST)
  if (currentThemeFlavor != cachedTheme) {
    Serial.println("[System] Theme flavor changed. Rebooting to regenerate cache...");
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

  delay(800);

  tft.fillScreen(CTP_BASE);
  renderScaledJpg(fileCache.getCurrent().c_str());
}

void setup() {
  Serial.begin(115200);
  Serial.println("[System] Booting ESP32 CYD Photo Frame...");
  Serial.println("[System] Type 'clear' or 'clear_cache' in the serial terminal to empty the cache.");

  // Load settings from NVS
  int cachedTheme = 0;
  {
    Preferences prefs;
    prefs.begin("settings", false);
    unsigned long loadedDelay = slideshowTimer.getInterval();
    HardwareLogic::loadSettings(prefs, currentBrightness, isAutoBrightness, loadedDelay, isRandomMode, showFilename, isInactivitySleep, currentThemeFlavor);
    slideshowTimer.setInterval(loadedDelay);
    cachedTheme = (int)prefs.getUInt("cached_theme", 0);
    prefs.end();
    Serial.printf("[System] Settings loaded from NVS. Brightness: %d, Auto: %d, Delay: %lu ms, Random: %d, ShowFN: %d, Sleep: %d, Theme: %d, CachedTheme: %d\n",
                  currentBrightness, isAutoBrightness, loadedDelay, isRandomMode, showFilename, isInactivitySleep, currentThemeFlavor, cachedTheme);
  }

  lastTouchTimeMs = millis();

  // Initialize LDR Sensor Pin
  pinMode(34, INPUT);

  // Initialize TFT
  tft.begin();
  tft.setRotation(1); // Landscape orientation

  // Initialize Touch Screen
  TouchManager::begin();

  // Initialize LVGL
  LVGLManager::init(tft.width(), tft.height());
  LVGLManager::setExitCallback(triggerExitSettings);

  // Initialize backlight control
#if defined(TFT_BL) && (TFT_BL >= 0)
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, currentBrightness);
#endif

  // Show calculating screen as soon as backlight is on
  drawCalculating();

  // Initialize the TJpg_Decoder
  TJpgDec.setCallback(tft_output);
  TJpgDec.setSwapBytes(true);

  // Initialize SD Card with fallback frequencies
  Serial.println("Mounting SD Card...");
  bool sdSuccess = false;
  uint32_t frequencies[] = {20000000UL, 10000000UL, 4000000UL};
  
#if defined(SD_SCK_PIN) && defined(SD_MISO_PIN) && defined(SD_MOSI_PIN) && defined(SD_CS_PIN)
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
#endif

  for (uint32_t freq : frequencies) {
    for (int retry = 1; retry <= 2; retry++) {
      if (SD.begin(SD_CS_PIN, SPI, freq, "/sd", 5, true)) {
        sdSuccess = true;
        Serial.printf("[SD] Mounted successfully at %lu MHz.\n", freq / 1000000UL);
        break;
      }
      Serial.printf("[SD] Mount failed at %lu MHz (attempt %d). Retrying...\n", freq / 1000000UL, retry);
      delay(100);
    }
    if (sdSuccess) break;
  }

  if (!sdSuccess) {
    showSDError(); // Blocks execution here if failed
  }
  Serial.println("SD Card mounted successfully.");

  // Populate cache
  populateCache();
  if (fileCache.isEmpty()) {
    Serial.println("Error: No images found on SD card.");
    tft.fillScreen(CTP_BASE);
    tft.setTextColor(CTP_RED, CTP_BASE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("No Images Found", tft.width() / 2, tft.height() / 2, 4);
    while(true) delay(1000); // Halt execution
  }

  // Ensure cache directory exists
  if (!SD.exists("/cache")) {
    SD.mkdir("/cache");
  }

  // Clean up any leftover temporary files from aborted runs
  File cacheDir = SD.open("/cache");
  if (cacheDir) {
    File file = cacheDir.openNextFile();
    while (file) {
      String path = file.path();
      file.close();
      if (path.endsWith(".tmp")) {
        Serial.printf("[System] Cleaning up orphaned temp file: %s\n", path.c_str());
        SD.remove(path.c_str());
      }
      file = cacheDir.openNextFile();
    }
    cacheDir.close();
  }

  // Clear cache if theme changed to regenerate background borders in the new flavor
  if (currentThemeFlavor != cachedTheme) {
    Serial.println("[System] Theme flavor changed. Clearing cache to regenerate borders...");
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
    }
    
    // Save new cached theme flavor in NVS
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putUInt("cached_theme", (uint32_t)currentThemeFlavor);
    prefs.end();
    cachedTheme = currentThemeFlavor; // Sync local variable
  }

  // Scan for files that need optimization/caching
  std::vector<std::pair<std::string, std::string>> filesToCache;
  unsigned long lastTouchCheckMs = 0;
  for (size_t i = 0; i < fileCache.size(); i++) {
    // Poll touch screen at most once every 50ms to prevent XPT2046 bus lockup
    unsigned long loopNow = millis();
    if (loopNow - lastTouchCheckMs >= 50) {
      lastTouchCheckMs = loopNow;
      if (TouchManager::isTouched()) {
        int tx = 0, ty = 0;
        if (TouchManager::getTouchPoint(tx, ty)) {
          Serial.printf("[Touch Debug] Touch detected during calculation at X=%d, Y=%d\n", tx, ty);
          if (isCancelButtonTouched(tx, ty)) {
            Serial.println("[System] Optimization cancelled during calculation.");
            optimizationCancelled = true;
            restrictCacheToExisting();
            filesToCache.clear();
            break;
          }
        }
      }
    }

    std::string originalPath = fileCache.getAt(i);
    std::string cachePath = getCachePath(originalPath);
    bool cacheValid = false;
    if (SD.exists(cachePath.c_str())) {
      File checkFile = SD.open(cachePath.c_str(), FILE_READ);
      if (checkFile) {
        if (checkFile.size() == 153600) {
          cacheValid = true;
        }
        checkFile.close();
      }
      if (!cacheValid) {
        Serial.printf("[System] Found invalid cache size for %s. Deleting...\n", cachePath.c_str());
        SD.remove(cachePath.c_str());
      }
    }
    if (!cacheValid) {
      filesToCache.push_back({originalPath, cachePath});
    }
  }

  // If there are files to cache, display progress bar and generate them silently
  if (!filesToCache.empty()) {
    Serial.printf("[System] Found %zu images that need optimization/caching.\n", filesToCache.size());
#if defined(TFT_BL) && (TFT_BL >= 0)
    // Make sure backlight is on during progress bar display
    analogWrite(TFT_BL, currentBrightness);
#endif
    
    optimizationCancelled = false; // Reset before starting optimization loop
    bool cancelled = false;
    unsigned long lastOptTouchCheckMs = 0;
    for (size_t i = 0; i < filesToCache.size(); i++) {
      // Poll touch screen between files (rate-limited to 50ms)
      unsigned long optNow = millis();
      if (optNow - lastOptTouchCheckMs >= 50) {
        lastOptTouchCheckMs = optNow;
        if (TouchManager::isTouched()) {
          int tx = 0, ty = 0;
          if (TouchManager::getTouchPoint(tx, ty)) {
            Serial.printf("[Touch Debug] Touch detected between files at X=%d, Y=%d\n", tx, ty);
            if (isCancelButtonTouched(tx, ty)) {
              Serial.println("[System] Optimization cancelled by user.");
              cancelled = true;
              break;
            }
          }
        }
      }

      if (optimizationCancelled) {
        cancelled = true;
        break;
      }

      const char* displayName = strrchr(filesToCache[i].first.c_str(), '/');
      displayName = displayName ? displayName + 1 : filesToCache[i].first.c_str();
      
      // Update progress bar
      drawProgress(i, filesToCache.size(), displayName);
      
      // Perform silent resizing and cache write
      generateCache(filesToCache[i].first.c_str(), filesToCache[i].second.c_str());

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
      delay(500);
    }
  }

  // Find and render the first valid image
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
    tft.fillScreen(CTP_BASE);
    tft.setTextColor(CTP_RED, CTP_BASE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("All Images Failed", tft.width() / 2, tft.height() / 2, 4);
    while(true) delay(1000); // Halt execution
  }

  slideshowTimer.start(millis());
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
  if (pendingExitSettings) {
    pendingExitSettings = false;
    exitSettings();
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
      Serial.println("[System] Entering inactivity sleep.");
    }
  }

  TouchZone zone = touchHandler.processTouch(touched, rawX, rawY, now);
  if (currentState == STATE_SLIDESHOW) {
    if (zone != TouchZone::NONE) {
      if (zone == TouchZone::LONG_PRESS_MID_CENTER) {
        Serial.println("[Touch] Long Press Center - Entering Settings Menu");
        currentState = STATE_SETTINGS;
        slideshowTimer.setPaused(true);
        tft.fillScreen(CTP_BASE);
        LVGLManager::showSettings();
      } else if (zone == TouchZone::MID_LEFT) {
        Serial.println("[Touch] Mid-Left tapped - Previous Image");
        if (!transitionPending) {
          transitionPending = true;
          pendingDirection = DIR_PREV;
          fader.startFade(currentBrightness, 0, 300);
          slideshowTimer.setPaused(true);
        }
      } else if (zone == TouchZone::MID_RIGHT) {
        Serial.println("[Touch] Mid-Right tapped - Next Image");
        if (!transitionPending) {
          transitionPending = true;
          pendingDirection = DIR_NEXT;
          fader.startFade(currentBrightness, 0, 300);
          slideshowTimer.setPaused(true);
        }
      } else if (zone == TouchZone::MID_CENTER) {
        bool isPaused = !slideshowTimer.isPaused();
        slideshowTimer.setPaused(isPaused);
        Serial.printf("[Touch] Mid-Center tapped - %s Slideshow\n", isPaused ? "Paused" : "Resumed");
        if (!isPaused) {
          slideshowTimer.reset(now);
        }
      } else if (zone == TouchZone::TOP_LEFT) {
        currentBrightness = min(currentBrightness + 25, 255);
        Serial.printf("[Touch] Top-Left tapped - Brightness increased to %d\n", currentBrightness);
        saveConfig();
      } else if (zone == TouchZone::BOTTOM_LEFT) {
        currentBrightness = max(currentBrightness - 25, 25);
        Serial.printf("[Touch] Bottom-Left tapped - Brightness decreased to %d\n", currentBrightness);
        saveConfig();
      } else if (zone == TouchZone::TOP_CENTER) {
        showFilename = !showFilename;
        Serial.printf("[Touch] Top-Center tapped - Filename display: %s\n", showFilename ? "ON" : "OFF");
        saveConfig();
        renderScaledJpg(fileCache.getCurrent().c_str());
      } else if (zone == TouchZone::BOTTOM_CENTER) {
        isRandomMode = !isRandomMode;
        Serial.printf("[Touch] Bottom-Center tapped - Random mode: %s\n", isRandomMode ? "ON" : "OFF");
        saveConfig();
      } else if (zone == TouchZone::TOP_RIGHT) {
        unsigned long currentDelay = slideshowTimer.getInterval();
        currentDelay = min(currentDelay + 1000, 60000UL);
        slideshowTimer.setInterval(currentDelay);
        Serial.printf("[Touch] Top-Right tapped - Delay increased to %lu ms\n", currentDelay);
        saveConfig();
      } else if (zone == TouchZone::BOTTOM_RIGHT) {
        unsigned long currentDelay = slideshowTimer.getInterval();
        currentDelay = max(currentDelay - 1000, 2000UL);
        slideshowTimer.setInterval(currentDelay);
        Serial.printf("[Touch] Bottom-Right tapped - Delay decreased to %lu ms\n", currentDelay);
        saveConfig();
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

  // Tick the fader and obtain current interpolated brightness
  int fadeBrightness = currentBrightness;
  bool fadeDone = fader.update(now, fadeBrightness);

  if (transitionPending && fader.getState() == BacklightFader::STATE_IDLE && fadeDone) {
    if (fadeBrightness == 0) {
      // Fade out complete, render next image
      if (pendingDirection == DIR_NEXT) {
        showNextImage();
      } else {
        showPreviousImage();
      }
      // Start fading back in
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
  delay(50);
}
