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

// Initialize the TFT object. 
// Note: Pins and drivers are automatically handled by platformio.ini build_flags!
TFT_eSPI tft = TFT_eSPI();
FileCache fileCache(64);
SlideshowTimer slideshowTimer(10000);
TouchHandler touchHandler(TFT_HEIGHT, TFT_WIDTH);

int currentBrightness = 255;
bool showFilename = true;
bool isRandomMode = false;
bool isAutoBrightness = false;
bool isInactivitySleep = false;

enum AppState {
  STATE_SLIDESHOW,
  STATE_SETTINGS
};
AppState currentState = STATE_SLIDESHOW;

// SD Card Chip Select for CYD
// const uint8_t SD_CS_PIN = 5;

// RGB565 color approximations for Catppuccin Mocha
#define CTP_BASE 0x18E5 
#define CTP_RED  0xF475
#define CTP_TEXT 0xCE79

/**
 * @brief Callback function required by TJpg_Decoder.
 * Pushes decompressed MCU blocks (usually 16x16 pixels) to the screen.
 */
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  // Stop drawing if it goes beyond the screen height
  if (y >= tft.height()) return 0;
  
  // Push the block of pixels to the TFT
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
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

/**
 * @brief Reads the JPEG header, determines the optimal hardware scale 
 * factor (1, 2, 4, or 8), calculates centering offsets, and renders it.
 * @return true if drawing succeeded, false otherwise.
 */
bool renderScaledJpg(const char* filename) {
  uint16_t img_w = 0, img_h = 0;

  Serial.printf("\n--- Rendering: %s ---\n", filename);

  // 1. Read the JPEG header (Returns 0 on success)
  uint16_t result = TJpgDec.getSdJpgSize(&img_w, &img_h, filename);
  
  if (result == 0) { // 0 equals JDR_OK
    
    // 2. Calculate the scaling ratio needed to fit the screen
    float ratio_w = (float)img_w / tft.width();
    float ratio_h = (float)img_h / tft.height();
    
    // Use the largest ratio to ensure the whole image fits on screen
    float max_ratio = max(ratio_w, ratio_h);

    // 3. Determine the optimal TJpgDec scale factor
    uint8_t scale = 1;
    if (max_ratio >= 8.0) {
      scale = 8;
    } else if (max_ratio >= 4.0) {
      scale = 4;
    } else if (max_ratio >= 2.0) {
      scale = 2;
    }

    Serial.printf("Original Size: %dx%d | Screen Size: %dx%d | Scale Factor: 1/%d\n", 
                  img_w, img_h, tft.width(), tft.height(), scale);

    // Apply the scaling factor to the hardware decoder
    TJpgDec.setJpgScale(scale);

    // 4. Calculate X and Y offsets to center the image
    int16_t scaled_w = img_w / scale;
    int16_t scaled_h = img_h / scale;
    
    int16_t x_offset = (tft.width() - scaled_w) / 2;
    int16_t y_offset = (tft.height() - scaled_h) / 2;

    if (x_offset < 0) x_offset = 0;
    if (y_offset < 0) y_offset = 0;

    // Clear the screen to the Catppuccin base color
    tft.fillScreen(CTP_BASE);

    // 5. Draw the image
    uint16_t drawResult = TJpgDec.drawSdJpg(x_offset, y_offset, filename);
    if(drawResult != 0) {
      Serial.printf("Error during draw: JRESULT %d\n", drawResult);
      return false;
    } else {
      if (showFilename) {
        tft.setTextColor(CTP_TEXT, CTP_BASE);
        tft.setTextDatum(BC_DATUM);
        const char* namePtr = strrchr(filename, '/');
        const char* displayName = namePtr ? namePtr + 1 : filename;
        tft.drawString(displayName, tft.width() / 2, tft.height() - 10, 2);
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

void exitSettings() {
  currentState = STATE_SLIDESHOW;
  slideshowTimer.setPaused(false);
  slideshowTimer.reset(millis());
  Serial.println("[System] Exiting settings menu. Resuming slideshow.");
  
  // Re-draw current photo to clear settings screen artifacts
  LVGLManager::hideSettings();
  tft.fillScreen(CTP_BASE);
  renderScaledJpg(fileCache.getCurrent().c_str());
}

void setup() {
  Serial.begin(115200);
  Serial.println("[System] Booting ESP32 CYD Photo Frame...");

  // Initialize TFT
  tft.begin();
  tft.setRotation(1); // Landscape orientation

  // Initialize LVGL
  LVGLManager::init(tft.width(), tft.height());
  LVGLManager::setExitCallback(exitSettings);

  // Initialize backlight control
#if defined(TFT_BL) && (TFT_BL >= 0)
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, currentBrightness);
#endif

  // Initialize Touch Screen
  TouchManager::begin();

  // Initialize the TJpg_Decoder
  TJpgDec.setCallback(tft_output);
  TJpgDec.setSwapBytes(true);

  // Initialize SD Card
  Serial.println("Mounting SD Card...");
  if (!SD.begin(SD_CS_PIN)) {
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
  if (fileCache.isEmpty()) {
    delay(1000);
    return;
  }

  // Check for touch input
  bool touched = TouchManager::isTouched();
  int rawX = 0, rawY = 0;
  if (touched) {
    TouchManager::getTouchPoint(rawX, rawY);
  }

  TouchZone zone = touchHandler.processTouch(touched, rawX, rawY, millis());
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
        showPreviousImage();
        slideshowTimer.reset(millis());
      } else if (zone == TouchZone::MID_RIGHT) {
        Serial.println("[Touch] Mid-Right tapped - Next Image");
        showNextImage();
        slideshowTimer.reset(millis());
      } else if (zone == TouchZone::MID_CENTER) {
        bool isPaused = !slideshowTimer.isPaused();
        slideshowTimer.setPaused(isPaused);
        Serial.printf("[Touch] Mid-Center tapped - %s Slideshow\n", isPaused ? "Paused" : "Resumed");
        if (!isPaused) {
          slideshowTimer.reset(millis());
        }
      } else if (zone == TouchZone::TOP_LEFT) {
        currentBrightness = min(currentBrightness + 25, 255);
#if defined(TFT_BL) && (TFT_BL >= 0)
        analogWrite(TFT_BL, currentBrightness);
#endif
        Serial.printf("[Touch] Top-Left tapped - Brightness increased to %d\n", currentBrightness);
      } else if (zone == TouchZone::BOTTOM_LEFT) {
        currentBrightness = max(currentBrightness - 25, 25);
#if defined(TFT_BL) && (TFT_BL >= 0)
        analogWrite(TFT_BL, currentBrightness);
#endif
        Serial.printf("[Touch] Bottom-Left tapped - Brightness decreased to %d\n", currentBrightness);
      } else if (zone == TouchZone::TOP_CENTER) {
        showFilename = !showFilename;
        Serial.printf("[Touch] Top-Center tapped - Filename display: %s\n", showFilename ? "ON" : "OFF");
        renderScaledJpg(fileCache.getCurrent().c_str());
      } else if (zone == TouchZone::BOTTOM_CENTER) {
        isRandomMode = !isRandomMode;
        Serial.printf("[Touch] Bottom-Center tapped - Random mode: %s\n", isRandomMode ? "ON" : "OFF");
      } else if (zone == TouchZone::TOP_RIGHT) {
        unsigned long currentDelay = slideshowTimer.getInterval();
        currentDelay = min(currentDelay + 1000, 60000UL);
        slideshowTimer.setInterval(currentDelay);
        Serial.printf("[Touch] Top-Right tapped - Delay increased to %lu ms\n", currentDelay);
      } else if (zone == TouchZone::BOTTOM_RIGHT) {
        unsigned long currentDelay = slideshowTimer.getInterval();
        currentDelay = max(currentDelay - 1000, 2000UL);
        slideshowTimer.setInterval(currentDelay);
        Serial.printf("[Touch] Bottom-Right tapped - Delay decreased to %lu ms\n", currentDelay);
      }
    }

    if (slideshowTimer.isElapsed(millis())) {
      showNextImage();
      slideshowTimer.reset(millis());
    }
  } else {
    // In STATE_SETTINGS, clear touchHandler's state machine
    touchHandler.processTouch(false, 0, 0, millis());
  }

  LVGLManager::handle();
  delay(50);
}
