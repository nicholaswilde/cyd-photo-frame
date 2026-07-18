# Plan: LVGL Settings and Features

## Phase 1: Project Integration & LVGL Setup [checkpoint: 40d5870]
- [x] Task: Configure PlatformIO and build environment a512705
  - [x] Add `lvgl/lvgl@^8.3.11` dependency to platformio.ini for both cyd_28r and cyd_35c
  - [x] Configure target build flags and SPI/I2C overrides for LVGL compatibility
- [x] Task: Implement host-native mock tests for LVGL initialization a512705
  - [x] Add basic unit tests under tests/ to verify display driver configuration structures
- [x] Task: Initialize LVGL and implement Drivers a512705
  - [x] Create include/lv_conf.h based on the v8 template
  - [x] Create display flush callback (bind LVGL to TFT_eSPI)
  - [x] Create input read callback (bind LVGL to TouchManager)
- [x] Task: Integrate LVGL Task Handler in Main Loop a512705
  - [x] Update src/main.cpp to call lv_tick_inc() and lv_timer_handler() periodically
- [x] Task: Phase Verification & Checkpoint (Refer to workflow.md) 40d5870

## Phase 2: Secret Gesture & Settings UI Layout
- [x] Task: Implement host-native unit tests for gesture detection and state changes b66e641
  - [x] Write tests verifying that a long-press triggers the settings state transition
- [x] Task: Implement Secret Long-Press Gesture 7e1fe68
  - [x] Add coordinates and duration tracking to touch processing in touch_handler b66e641
  - [x] Handle state transitions (STATE_SLIDESHOW <-> STATE_SETTINGS) in src/main.cpp
- [x] Task: Build LVGL Settings Page UI 7e1fe68
  - [x] Initialize settings screen using Catppuccin theme colors
  - [x] Add a scrollable page containing a slider for Brightness, switches for Random Mode and Filename Display, a slider for Slideshow Delay, and a switch for Inactivity Sleep
  - [x] Implement EXIT/Save button to close Settings UI and return to slideshow
- [x] Task: Phase Verification & Checkpoint (Refer to workflow.md) 9f16ef2

## Phase 3: Hardware Integrations & Features
- [x] Task: Write host-native unit tests for Preferences persistence and LDR auto-brightness e11918f
  - [x] Implement mock Preferences class and test saving/restoring configurations
  - [x] Test auto-brightness mapping calculations
- [x] Task: Implement Settings Persistence 4dea98b
  - [x] Add Preferences.h calls to read configurations on boot
  - [x] Write values to NVS when settings widgets are adjusted
- [x] Task: Implement LDR light-sensor Auto-Brightness 643ddfb
  - [x] Add LDR GPIO 34 analog reading loop
  - [x] Map sensor readings to backlight duty cycle, automatically updating the hardware
- [x] Task: Implement Inactivity and Darkness Sleep Scheduler 2f6cc4a
  - [x] Track last touch time; shut off display backlight if 5 minutes pass with no touches (if enabled)
  - [x] Put screen to sleep if LDR detects absolute darkness for 5 minutes
  - [x] Restore backlight on touch interrupts
- [ ] Task: Phase Verification & Checkpoint (Refer to workflow.md)

## Phase 4: Visual Polishing & Transitions
- [ ] Task: Write host-native unit tests for backlight fade states
  - [ ] Verify fade transition timing calculations and step interpolation
- [ ] Task: Implement Fade-to-Black transitions
  - [ ] Add non-blocking smooth dimming transition before next image decode
  - [ ] Fade back up to target brightness after draw is finished
- [ ] Task: Run final static analysis and Quality Gate checks
  - [ ] Run pio check and verify no errors remain
- [ ] Task: Phase Verification & Checkpoint (Refer to workflow.md)
