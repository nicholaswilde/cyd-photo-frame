#ifndef LED_MANAGER_H
#define LED_MANAGER_H

class LedManager {
public:
    enum State {
        STATE_OFF,           // LED off (sleep/dark)
        STATE_BOOT,          // Yellow slow blink — booting
        STATE_OPTIMIZING,    // Blue fast blink  — converting/caching images
        STATE_SLIDESHOW,     // Green solid       — slideshow running normally
        STATE_TRANSITION,    // White 1s pulse    — image transition
        STATE_SETTINGS,      // Purple solid      — settings screen open
        STATE_ERROR,         // Red solid         — fatal error (SD fail etc.)
        STATE_SCREENSHOT,    // Cyan 1s pulse     — screenshot being taken
    };

    LedManager(int redPin, int greenPin, int bluePin);
    void begin();
    void update(unsigned long currentMillis);
    void setState(State state);
    State getState() const;
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setBrightness(int brightness);
    int getBrightness() const;

private:
    void writePins(bool r, bool g, bool b);
    void turnOffAll();

    int _redPin;
    int _greenPin;
    int _bluePin;
    State _state;
    bool _enabled;
    int _brightness;

    unsigned long _lastToggleTime;
    bool _blinkState;
    unsigned long _stateStartTime;
    bool _stateInitialized;
};

#endif // LED_MANAGER_H
