# Plan: LVGL Settings and Features

## Phase 1: Project Integration & LVGL Setup
- [ ] Task: Configure PlatformIO and build environment
  - [ ] Add `lvgl/lvgl@^8.3.11` dependency to platformio.ini for both cyd_28r and cyd_35c
  - [ ] Configure target build flags and SPI/I2C overrides for LVGL compatibility
- [ ] Task: Implement host-native mock tests for LVGL initialization
  - [ ] Add basic unit tests under tests/ to verify display driver configuration structures
- [ ] Task: Initialize LVGL and implement Drivers
  - [ ] Create include/lv_conf.h based on the v8 template
  - [ ] Create display flush callback (bind LVGL to TFT_eSPI)
  - [ ] Create input read callback (bind LVGL to TouchManager)
- [ ] Task: Integrate LVGL Task Handler in Main Loop
  - [ ] Update src/main.cpp to call lv_tick_inc() and lv_timer_handler() periodically
- [ ] Task: Phase Verification & Checkpoint (Refer to workflow.md)

## Phase 2: Secret Gesture & Settings UI Layout
- [ ] Task: Implement host-native unit tests for gesture detection and state changes
  - [ ] Write tests verifying that a long-press triggers the settings state transition
- [ ] Task: Implement Secret Long-Press Gesture
  - [ ] Add coordinates and duration tracking to touch processing in touch_handler
  - [ ] Handle state transitions (STATE_SLIDESHOW <-> STATE_SETTINGS) in src/main.cpp
- [ ] Task: Build LVGL Settings Page UI
  - [ ] Initialize settings screen using Catppuccin theme colors
  - [ ] Add a scrollable page containing a slider for Brightness, switches for Random Mode and Filename Display, a slider for Slideshow Delay, and a switch for Inactivity Sleep
  - [ ] Implement EXIT/Save button to close Settings UI and return to slideshow
- [ ] Task: Phase Verification & Checkpoint (Refer to workflow.md)

## Phase 3: Hardware Integrations & Features
- [ ] Task: Write host-native unit tests for Preferences persistence and LDR auto-brightness
  - [ ] Implement mock Preferences class and test saving/restoring configurations
  - [ ] Test auto-brightness mapping calculations
- [ ] Task: Implement Settings Persistence
  - [ ] Add Preferences.h calls to read configurations on boot
  - [ ] Write values to NVS when settings widgets are adjusted
- [ ] Task: Implement LDR light-sensor Auto-Brightness
  - [ ] Add LDR GPIO 34 analog reading loop
  - [ ] Map sensor readings to backlight duty cycle, automatically updating the hardware
- [ ] Task: Implement Inactivity and Darkness Sleep Scheduler
  - [ ] Track last touch time; shut off display backlight if 5 minutes pass with no touches (if enabled)
  - [ ] Put screen to sleep if LDR detects absolute darkness for 5 minutes
  - [ ] Restore backlight on touch interrupts
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
