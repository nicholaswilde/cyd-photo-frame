# Specification: LVGL Settings and Features

## 1. Overview
Introduce LVGL (Light and Versatile Graphics Library) version 8 to manage a scrollable Settings Menu. The settings menu will be hidden behind a secret long-press gesture. Additional hardware features like LDR auto-brightness, settings persistence, fade-to-black transitions, and a sleep scheduler will be implemented and integrated with the settings.

## 2. Functional Requirements
*   **LVGL Core Integration:**
    *   Integrate LVGL v8.4.x into the project via `platformio.ini`.
    *   Create a valid `lv_conf.h` and initialize LVGL.
    *   Register display flush callback using `TFT_eSPI`.
    *   Register touch input device callback using `TouchManager`.
    *   Integrate tick and timer handler updates in the main Arduino `loop()`.
*   **Secret Settings Trigger:**
    *   Detect a long-press (held for $\ge 1.5$ seconds) in the `MID_CENTER` region of the touchscreen during the slideshow.
    *   When triggered, pause the slideshow timer and display the LVGL Settings Menu.
*   **LVGL Settings UI:**
    *   Render a full-screen, clean, scrollable list of configurations styled using the Catppuccin theme:
        *   **Manual Brightness:** Slider to adjust backlight duty cycle (10% to 100%).
        *   **Auto-Brightness (LDR):** Switch to toggle LDR light sensor feedback. If enabled, the manual brightness slider is disabled.
        *   **Slideshow Delay:** Dropdown or slider to select photo rotation delay (2s to 60s).
        *   **Random Mode:** Switch to toggle random order vs. sequential.
        *   **Show Filename:** Switch to toggle filename drawing at the bottom.
        *   **Inactivity Sleep:** Switch to toggle auto-sleep after 5 minutes of no touch.
        *   **Close & Exit Button:** Closes the settings menu and resumes the slideshow.
*   **Hardware & Core Integrations:**
    *   **LDR Auto-Brightness:** Periodic reading of LDR analog input on GPIO 34. Map LDR readings to backlight PWM values when LDR mode is active.
    *   **Settings Persistence:** Initialize `<Preferences.h>` and save all states on modification. Load these saved preferences at boot time in `setup()`.
    *   **Fade-to-Black Transitions:** Dim the backlight PWM smoothly to 0 before rendering the next image, and fade it back up to target brightness once rendering is complete.
    *   **Screen Sleep Scheduler:** Turn off display backlight (write PWM 0) if 5 minutes pass with no touch, or if the LDR registers absolute darkness for more than 5 minutes. Tap anywhere to wake the screen.

## 3. Non-Functional Requirements
*   **Memory Efficiency:** Ensure display buffers and LVGL heap are small enough to run without PSRAM.
*   **State Separation:** The rendering of slideshow photos must bypass LVGL to draw directly via `TJpg_Decoder`, and the screen must be fully cleared before loading LVGL UI elements to avoid overlay corruption.

## 4. Acceptance Criteria
*   Pressing the center of the screen for $\ge 1.5$ seconds suspends the slideshow and loads the LVGL settings panel.
*   Modifying sliders/switches updates system behavior immediately (e.g. brightness changes when sliding).
*   Settings are preserved after power cycling.
*   Slideshow images change using a smooth backlight fade transition.
*   Covered by unit tests where appropriate (e.g. checking state transitions and config validation).

## 5. Out of Scope
*   Wi-Fi credentials configuration or remote image downloading.
*   Alternative transition animations (e.g., sliding or wiping) since they require double-buffering the full-resolution frame.
