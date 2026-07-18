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
    
    // Determine row (vertical splitting: top 1/4, bottom 1/4, middle 1/2)
    bool isTopRow = (pixelY < m_displayHeight / 4);
    bool isBottomRow = (pixelY >= (3 * m_displayHeight) / 4);
    
    // Determine column (horizontal splitting: 3 equal columns)
    bool isLeftCol = (pixelX < m_displayWidth / 3);
    bool isRightCol = (pixelX >= (2 * m_displayWidth) / 3);
    
    if (isTopRow) {
        if (isLeftCol) return TouchZone::TOP_LEFT;
        if (isRightCol) return TouchZone::TOP_RIGHT;
        return TouchZone::TOP_CENTER;
    } else if (isBottomRow) {
        if (isLeftCol) return TouchZone::BOTTOM_LEFT;
        if (isRightCol) return TouchZone::BOTTOM_RIGHT;
        return TouchZone::BOTTOM_CENTER;
    } else {
        if (isLeftCol) return TouchZone::MID_LEFT;
        if (isRightCol) return TouchZone::MID_RIGHT;
        return TouchZone::MID_CENTER;
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
