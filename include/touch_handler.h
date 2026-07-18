#ifndef TOUCH_HANDLER_H
#define TOUCH_HANDLER_H

enum class TouchZone {
    NONE,
    TOP_LEFT,
    TOP_CENTER,
    TOP_RIGHT,
    MID_LEFT,
    MID_CENTER,
    MID_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_CENTER,
    BOTTOM_RIGHT
};

class TouchHandler {
public:
    TouchHandler(int displayWidth, int displayHeight, unsigned long debounceMs = 300);
    
    TouchZone processTouch(bool isTouched, int rawX, int rawY, unsigned long currentTimeMs);
    void mapCoordinates(int rawX, int rawY, int& pixelX, int& pixelY, bool isCapacitive);

private:
    int m_displayWidth;
    int m_displayHeight;
    unsigned long m_debounceMs;
    unsigned long m_lastTapTimeMs;
};

#endif // TOUCH_HANDLER_H
