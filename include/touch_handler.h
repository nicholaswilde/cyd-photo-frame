#ifndef TOUCH_HANDLER_H
#define TOUCH_HANDLER_H

enum class TouchZone {
    NONE,
    TOP_LEFT,
    TOP_CENTER,
    TOP_RIGHT,
    MID_LEFT,
    MID_CENTER_DOWN,
    MID_CENTER,
    MID_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_CENTER,
    BOTTOM_RIGHT,
    LONG_PRESS_MID_CENTER
};

class TouchHandler {
public:
    TouchHandler(int displayWidth, int displayHeight, unsigned long debounceMs = 300);
    
    TouchZone processTouch(bool isTouched, int rawX, int rawY, unsigned long currentTimeMs);
    void mapCoordinates(int rawX, int rawY, int& pixelX, int& pixelY, bool isCapacitive);
    void setOrientation(int orientation) {
        m_orientation = orientation;
        int maxDim = m_displayWidth > m_displayHeight ? m_displayWidth : m_displayHeight;
        int minDim = m_displayWidth < m_displayHeight ? m_displayWidth : m_displayHeight;
        if (orientation == 0 || orientation == 2) {
            m_displayWidth = minDim;
            m_displayHeight = maxDim;
        } else {
            m_displayWidth = maxDim;
            m_displayHeight = minDim;
        }
    }

private:
    int m_displayWidth;
    int m_displayHeight;
    unsigned long m_debounceMs;
    unsigned long m_lastTapTimeMs;
    int m_orientation = 1;
    
    bool m_wasTouched = false;
    unsigned long m_touchStartTimeMs = 0;
    TouchZone m_touchStartZone = TouchZone::NONE;
    bool m_longPressTriggered = false;
};

#endif // TOUCH_HANDLER_H
