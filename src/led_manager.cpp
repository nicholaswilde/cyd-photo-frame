#include "led_manager.h"
#ifndef NATIVE_TEST
#include <Arduino.h>
#else
#include "Arduino.h"
#endif

// CYD RGB LED channel assignments (LEDC channels 5/6/7 to stay clear of TFT_BL)
static const int LED_CH_R = 5;
static const int LED_CH_G = 6;
static const int LED_CH_B = 7;

LedManager::LedManager(int redPin, int greenPin, int bluePin)
    : _redPin(redPin), _greenPin(greenPin), _bluePin(bluePin),
      _state(STATE_BOOT), _enabled(true), _brightness(255),
      _lastToggleTime(0), _blinkState(false), _stateStartTime(0),
      _stateInitialized(false) {}

void LedManager::begin() {
#ifndef NATIVE_TEST
    // Set up three LEDC PWM channels for dimming the RGB LED
    ledcSetup(LED_CH_R, 5000, 8); // 5 kHz, 8-bit
    ledcSetup(LED_CH_G, 5000, 8);
    ledcSetup(LED_CH_B, 5000, 8);
    ledcAttachPin(_redPin,   LED_CH_R);
    ledcAttachPin(_greenPin, LED_CH_G);
    ledcAttachPin(_bluePin,  LED_CH_B);
#else
    pinMode(_redPin,   OUTPUT);
    pinMode(_greenPin, OUTPUT);
    pinMode(_bluePin,  OUTPUT);
#endif
    turnOffAll();
    _stateInitialized = false;
}

/**
 * @brief Writes colour to the three pins, honouring brightness and active-LOW wiring.
 *        CYD RGB LED is common-anode (active-LOW): duty 255 = OFF, 0 = full brightness.
 */
void LedManager::writePins(bool r, bool g, bool b) {
    if (!_enabled) {
        turnOffAll();
        return;
    }
#ifndef NATIVE_TEST
    // Scale active duty by _brightness (0-255); invert for active-LOW.
    int rDuty = r ? (255 - _brightness) : 255;
    int gDuty = g ? (255 - _brightness) : 255;
    int bDuty = b ? (255 - _brightness) : 255;
    ledcWrite(LED_CH_R, rDuty);
    ledcWrite(LED_CH_G, gDuty);
    ledcWrite(LED_CH_B, bDuty);
#else
    digitalWrite(_redPin,   r ? LOW : HIGH);
    digitalWrite(_greenPin, g ? LOW : HIGH);
    digitalWrite(_bluePin,  b ? LOW : HIGH);
#endif
}

void LedManager::turnOffAll() {
#ifndef NATIVE_TEST
    ledcWrite(LED_CH_R, 255);
    ledcWrite(LED_CH_G, 255);
    ledcWrite(LED_CH_B, 255);
#else
    digitalWrite(_redPin,   HIGH);
    digitalWrite(_greenPin, HIGH);
    digitalWrite(_bluePin,  HIGH);
#endif
}

/**
 * @brief Non-blocking state machine — call every loop() iteration.
 *
 * State colour reference:
 *   STATE_BOOT        — Yellow slow blink (500 ms)  : device initialising
 *   STATE_OPTIMIZING  — Blue fast blink  (200 ms)  : converting/caching images
 *   STATE_SLIDESHOW   — Green solid                : running normally
 *   STATE_TRANSITION  — White 1 s pulse            : image switching
 *   STATE_SETTINGS    — Purple solid               : settings screen open
 *   STATE_ERROR       — Red solid                  : fatal error
 *   STATE_SCREENSHOT  — Cyan 1 s pulse             : screenshot capture in progress
 *   STATE_OFF         — All off                    : sleep / backlight off
 */
void LedManager::update(unsigned long currentMillis) {
    if (!_enabled) {
        turnOffAll();
        return;
    }

    if (!_stateInitialized) {
        _stateStartTime = currentMillis;
        _lastToggleTime = currentMillis;
        _blinkState = true;
        _stateInitialized = true;
    }

    switch (_state) {
        case STATE_OFF:
            writePins(false, false, false);
            break;

        case STATE_BOOT:
            // Yellow slow blink (500 ms) while booting
            if (currentMillis - _lastToggleTime >= 500) {
                _blinkState = !_blinkState;
                _lastToggleTime = currentMillis;
            }
            writePins(_blinkState, _blinkState, false); // R+G = Yellow
            break;

        case STATE_OPTIMIZING:
            // Blue fast blink (200 ms) during image conversion
            if (currentMillis - _lastToggleTime >= 200) {
                _blinkState = !_blinkState;
                _lastToggleTime = currentMillis;
            }
            writePins(false, false, _blinkState);
            break;

        case STATE_SLIDESHOW:
            // Green solid — normal operation
            writePins(false, true, false);
            break;

        case STATE_TRANSITION:
            // White 1 s pulse then back to SLIDESHOW
            if (currentMillis - _stateStartTime >= 1000) {
                _state = STATE_SLIDESHOW;
                _stateStartTime = currentMillis;
                writePins(false, true, false); // green
            } else {
                writePins(true, true, true); // white
            }
            break;

        case STATE_SETTINGS:
            // Purple solid — settings screen open
            writePins(true, false, true);
            break;

        case STATE_ERROR:
            // Red solid — fatal error
            writePins(true, false, false);
            break;

        case STATE_SCREENSHOT:
            // Cyan 1 s pulse then back to previous solid state
            if (currentMillis - _stateStartTime >= 1000) {
                _state = STATE_SLIDESHOW;
                _stateStartTime = currentMillis;
                writePins(false, true, false); // back to green
            } else {
                writePins(false, true, true); // cyan
            }
            break;
    }
}

void LedManager::setState(State state) {
    if (_state != state) {
        _state = state;
        _stateStartTime = 0;
        _lastToggleTime = 0;
        _blinkState = true;
        _stateInitialized = false;
    }
}

LedManager::State LedManager::getState() const {
    return _state;
}

void LedManager::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!_enabled) {
        turnOffAll();
    }
}

bool LedManager::isEnabled() const {
    return _enabled;
}

/**
 * @brief Sets the LED brightness (0–255) applied when a channel is active.
 *        0 = off, 255 = maximum brightness.
 */
void LedManager::setBrightness(int brightness) {
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    _brightness = brightness;
}

/**
 * @brief Returns the current LED brightness value (0–255).
 */
int LedManager::getBrightness() const {
    return _brightness;
}
