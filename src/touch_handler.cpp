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
        m_wasTouched = false;
        m_touchStartZone = TouchZone::NONE;
        m_longPressTriggered = false;
        return TouchZone::NONE;
    }
    
    int pixelX = 0;
    int pixelY = 0;
    
    bool isCapacitive = (rawX <= m_displayWidth * 2 && rawY <= m_displayHeight * 2);
    mapCoordinates(rawX, rawY, pixelX, pixelY, isCapacitive);
    
    // Determine row (vertical splitting: top 1/4, bottom 1/4, middle 1/2)
    bool isTopRow = (pixelY < m_displayHeight / 4);
    bool isBottomRow = (pixelY >= (3 * m_displayHeight) / 4);
    
    // Determine column (horizontal splitting: 3 equal columns)
    bool isLeftCol = (pixelX < m_displayWidth / 3);
    bool isRightCol = (pixelX >= (2 * m_displayWidth) / 3);
    
    TouchZone currentZone = TouchZone::NONE;
    if (isTopRow) {
        if (isLeftCol) currentZone = TouchZone::TOP_LEFT;
        else if (isRightCol) currentZone = TouchZone::TOP_RIGHT;
        else currentZone = TouchZone::TOP_CENTER;
    } else if (isBottomRow) {
        if (isLeftCol) currentZone = TouchZone::BOTTOM_LEFT;
        else if (isRightCol) currentZone = TouchZone::BOTTOM_RIGHT;
        else currentZone = TouchZone::BOTTOM_CENTER;
    } else {
        if (isLeftCol) currentZone = TouchZone::MID_LEFT;
        else if (isRightCol) currentZone = TouchZone::MID_RIGHT;
        else currentZone = TouchZone::MID_CENTER;
    }
    
    if (!m_wasTouched || currentZone != m_touchStartZone) {
        if (currentTimeMs - m_lastTapTimeMs < m_debounceMs) {
            return TouchZone::NONE;
        }
        m_wasTouched = true;
        m_touchStartTimeMs = currentTimeMs;
        m_touchStartZone = currentZone;
        m_longPressTriggered = false;
        m_lastTapTimeMs = currentTimeMs;
        return currentZone;
    } else {
        if (currentZone == m_touchStartZone && m_touchStartZone == TouchZone::MID_CENTER) {
            if (!m_longPressTriggered) {
                if (currentTimeMs - m_touchStartTimeMs >= 1500) {
                    m_longPressTriggered = true;
                    return TouchZone::LONG_PRESS_MID_CENTER;
                }
            }
        }
        return TouchZone::NONE;
    }
}

void TouchHandler::mapCoordinates(int rawX, int rawY, int& pixelX, int& pixelY, bool isCapacitive) {
    int w_land = m_displayWidth > m_displayHeight ? m_displayWidth : m_displayHeight;
    int h_land = m_displayWidth < m_displayHeight ? m_displayWidth : m_displayHeight;

    if (isCapacitive) {
        bool rawIsPortrait = (rawX <= h_land && rawY <= w_land);
        
        if (rawIsPortrait) {
            switch (m_orientation) {
                case 0: // Portrait Rev / Native Portrait
                    pixelX = rawX;
                    pixelY = rawY;
                    break;
                case 1: // Landscape
                    pixelX = rawY;
                    pixelY = h_land - rawX;
                    break;
                case 2: // Portrait
                    pixelX = h_land - rawX;
                    pixelY = w_land - rawY;
                    break;
                case 3: // Landscape Rev
                    pixelX = w_land - rawY;
                    pixelY = rawX;
                    break;
                default:
                    pixelX = rawY;
                    pixelY = rawX;
                    break;
            }
        } else {
            switch (m_orientation) {
                case 0: // Portrait
                    pixelX = h_land - rawY;
                    pixelY = w_land - rawX;
                    break;
                case 1: // Landscape
                    pixelX = rawX;
                    pixelY = rawY;
                    break;
                case 2: // Portrait Rev
                    pixelX = rawY;
                    pixelY = rawX;
                    break;
                case 3: // Landscape Rev
                    pixelX = w_land - rawX;
                    pixelY = h_land - rawY;
                    break;
                default:
                    pixelX = rawX;
                    pixelY = rawY;
                    break;
            }
        }
    } else {
        int lx = mapRangeClipped(rawX, 200, 3800, w_land);
        int ly = mapRangeClipped(rawY, 200, 3800, h_land);
        
        switch (m_orientation) {
            case 0: // Portrait Rev
                pixelX = h_land - ly;
                pixelY = lx;
                break;
            case 1: // Landscape
                pixelX = lx;
                pixelY = ly;
                break;
            case 2: // Portrait
                pixelX = ly;
                pixelY = w_land - lx;
                break;
            case 3: // Landscape Rev
                pixelX = w_land - lx;
                pixelY = h_land - ly;
                break;
            default:
                pixelX = lx;
                pixelY = ly;
                break;
        }
    }

    if (pixelX < 0) pixelX = 0;
    if (pixelX >= m_displayWidth) pixelX = m_displayWidth - 1;
    if (pixelY < 0) pixelY = 0;
    if (pixelY >= m_displayHeight) pixelY = m_displayHeight - 1;
}
