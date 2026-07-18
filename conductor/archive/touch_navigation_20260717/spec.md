# Specification: Implement Touch Screen Controls for Slideshow Navigation

## Overview
Currently, the photo frame slideshow runs as a non-interactive, infinite loop with a hardcoded 10-second delay between images. This track introduces interactive touch-based controls to allow user-driven navigation and playback control.

## Target Hardware
1. **CYD-28R**: SPI-based resistive touchscreen (XPT2046).
2. **CYD-35C**: I2C-based capacitive touchscreen (GT911/CST820).

## Functional Requirements
- **Touch-Based Navigation Zones (Landscape mode, 320x240 or 480x320)**:
  - **Left Area (0% - 30% width)**: Tap to show the **Previous** image.
  - **Center Area (30% - 70% width)**: Tap to **Pause** or **Resume** the slideshow. When paused, the display remains on the current image indefinitely.
  - **Right Area (70% - 100% width)**: Tap to show the **Next** image.
- **Responsiveness**:
  - Taps must be detected immediately. We must refactor the blocking `delay(10000)` in the main loop to a non-blocking poll loop (e.g., 50ms intervals) that checks for touch inputs.
- **Calibration / Debouncing**:
  - Touch inputs must be debounced (e.g., minimum 300ms between accepted tap actions) to prevent double-triggering.
  - Adjust for touch screen coordinates orientation.

## Technical Design
- **Conditional Compilation**:
  - Inject the driver dependencies and initializations based on build flags (`ILI9341_DRIVER` vs `ST7796_DRIVER` or driver selection).
  - For `cyd_28r`, include `<XPT2046_Touchscreen.h>`.
  - For `cyd_35c`, include an I2C touch driver wrapper or use wire interface to read CST820/GT911 registers.
- **Navigation State**:
  - Introduce an index/list structure or bidirectional filesystem traversal. Currently, the code uses `rootDir.openNextFile()` which only moves forward and cannot go backward. We must implement a file path caching mechanism (e.g., cache up to 32/64 file names) to support moving to the previous image.
