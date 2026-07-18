#include "touch_handler.h"

TouchHandler::TouchHandler(int displayWidth, int displayHeight, unsigned long debounceMs)
    : m_displayWidth(displayWidth), m_displayHeight(displayHeight), m_debounceMs(debounceMs), m_lastTapTimeMs(0) {}

static int mapRangeClipped(int x, int in_min, int in_max, int out_max) {
    if (in_max == in_min) return 0;
    long val = (long)(x - in_min) * out_max / (in_max - in_min);
    if (val < 0) return 0;
    if (val > out_max) return out_max;
    return (int)val;
}

TouchZone TouchHandler::processTouch(bool isTouched, int rawX, int rawY, unsigned long currentTimeMs) {
    if (!isTouched) {
        return TouchZone::NONE;
    }
    
    if (currentTimeMs - m_lastTapTimeMs < m_debounceMs) {
        return TouchZone::NONE;
    }
    
    int pixelX = 0;
    int pixelY = 0;
    
    bool isCapacitive = (rawX <= m_displayWidth * 2 && rawY <= m_displayHeight * 2);
    mapCoordinates(rawX, rawY, pixelX, pixelY, isCapacitive);
    
    m_lastTapTimeMs = currentTimeMs;
    
    if (pixelX < (m_displayWidth * 3) / 10) {
        return TouchZone::LEFT;
    } else if (pixelX >= (m_displayWidth * 7) / 10) {
        return TouchZone::RIGHT;
    } else {
        return TouchZone::CENTER;
    }
}

void TouchHandler::mapCoordinates(int rawX, int rawY, int& pixelX, int& pixelY, bool isCapacitive) {
    if (isCapacitive) {
        pixelX = rawX;
        pixelY = rawY;
    } else {
        pixelX = mapRangeClipped(rawX, 200, 3800, m_displayWidth);
        pixelY = mapRangeClipped(rawY, 200, 3800, m_displayHeight);
    }
}
