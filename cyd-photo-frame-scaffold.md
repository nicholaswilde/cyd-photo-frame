## Phase 7: Multi-Target Hardware Support
The system must support two specific hardware targets from a single unified codebase:
1. **CYD-28R:** ESP32-2432S028R (2.8", 320x240, ILI9341)
2. **CYD-35C:** ESP32-3248S035C (3.5", 480x320, ST7796, Capacitive Touch)

### 1. PlatformIO Configuration (`platformio.ini`)
- Do not use `User_Setup.h` for `TFT_eSPI`. Instead, inject the TFT configurations via `build_flags` for two separate environments.
- Create `[env:cyd_28r]` and `[env:cyd_35c]`.
- Set `USER_SETUP_LOADED=1` in both to bypass the default setup file.
- Define `TFT_WIDTH` and `TFT_HEIGHT` appropriately for each environment.
- Assign the correct driver macros: `ILI9341_DRIVER=1` for `cyd_28r` and `ST7796_DRIVER=1` for `cyd_35c`.
- **Note on Touch:** If ported UI features require touch, ensure the code conditionally compiles the XPT2046 SPI driver for `cyd_28r` and the GT911/CST820 I2C driver for `cyd_35c`.

### 2. Dynamic Screen Sizing in C++
- The image scaling logic must not hardcode `320` and `240`.
- Update the `renderScaledJpg` function to dynamically query the active display dimensions using `tft.width()` and `tft.height()`.

```cpp
// Updated renderScaledJpg for multi-target support
void renderScaledJpg(const char* filename) {
  uint16_t img_w = 0, img_h = 0;

  if (TJpg_Decoder.getJpgSize(&img_w, &img_h, filename)) {
    float ratio_w = (float)img_w / tft.width();
    float ratio_h = (float)img_h / tft.height();
    float max_ratio = max(ratio_w, ratio_h);

    uint8_t scale = 1;
    if (max_ratio >= 8.0) scale = 8;
    else if (max_ratio >= 4.0) scale = 4;
    else if (max_ratio >= 2.0) scale = 2;

    TJpg_Decoder.setJpgScale(scale);

    int16_t scaled_w = img_w / scale;
    int16_t scaled_h = img_h / scale;
    
    int16_t x_offset = (tft.width() - scaled_w) / 2;
    int16_t y_offset = (tft.height() - scaled_h) / 2;

    if (x_offset < 0) x_offset = 0;
    if (y_offset < 0) y_offset = 0;

    TJpg_Decoder.drawSdJpg(x_offset, y_offset, filename);
  } else {
    Serial.printf("Error: Could not read JPEG header for %s\n", filename);
  }
}
