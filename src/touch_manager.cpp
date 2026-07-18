#include "touch_manager.h"

#if defined(NATIVE_TEST)
static bool mockTouched = false;
static int mockX = 0;
static int mockY = 0;

void TouchManager::begin() {}
bool TouchManager::isTouched() { return mockTouched; }
bool TouchManager::getTouchPoint(int& x, int& y) {
    x = mockX;
    y = mockY;
    return mockTouched;
}
void setMockTouch(bool touched, int x, int y) {
    mockTouched = touched;
    mockX = x;
    mockY = y;
}
#elif defined(ILI9341_DRIVER)
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

static SPIClass touchSPI(VSPI);
static XPT2046_Touchscreen touch(XPT2046_CS, XPT2046_IRQ);

void TouchManager::begin() {
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touch.begin(touchSPI);
    touch.setRotation(1); // landscape
}

bool TouchManager::isTouched() {
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    return touch.touched();
}

bool TouchManager::getTouchPoint(int& x, int& y) {
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    if (!touch.touched()) {
        return false;
    }
    TS_Point p = touch.getPoint();
    x = p.x;
    y = p.y;
    return true;
}
#elif defined(ST7796_DRIVER)
#include <Arduino.h>
#include <Wire.h>

#define I2C_SDA 33
#define I2C_SCL 32
#define GT911_ADDR1 0x5D
#define GT911_ADDR2 0x14
#define CST820_ADDR  0x15

static uint8_t touchAddress = 0x5D;
static bool initialized = false;

void TouchManager::begin() {
    Wire.begin(I2C_SDA, I2C_SCL);
    
    Wire.beginTransmission(GT911_ADDR1);
    if (Wire.endTransmission() == 0) {
        touchAddress = GT911_ADDR1;
        initialized = true;
        Serial.println("[Touch] GT911 found at 0x5D");
        return;
    }
    Wire.beginTransmission(GT911_ADDR2);
    if (Wire.endTransmission() == 0) {
        touchAddress = GT911_ADDR2;
        initialized = true;
        Serial.println("[Touch] GT911 found at 0x14");
        return;
    }
    Wire.beginTransmission(CST820_ADDR);
    if (Wire.endTransmission() == 0) {
        touchAddress = CST820_ADDR;
        initialized = true;
        Serial.println("[Touch] CST820 found at 0x15");
        return;
    }
    Serial.println("[Touch] Error: Touch controller not found!");
}

bool TouchManager::isTouched() {
    if (!initialized) return false;
    if (touchAddress == GT911_ADDR1 || touchAddress == GT911_ADDR2) {
        Wire.beginTransmission(touchAddress);
        Wire.write(0x81);
        Wire.write(0x4E);
        if (Wire.endTransmission() != 0) return false;
        
        Wire.requestFrom(touchAddress, (uint8_t)1);
        if (Wire.available()) {
            uint8_t status = Wire.read();
            Wire.beginTransmission(touchAddress);
            Wire.write(0x81);
            Wire.write(0x4E);
            Wire.write(0x00);
            Wire.endTransmission();
            
            return (status & 0x80) && (status & 0x0F) > 0;
        }
    } else if (touchAddress == CST820_ADDR) {
        Wire.beginTransmission(touchAddress);
        Wire.write(0x02);
        if (Wire.endTransmission() != 0) return false;
        
        Wire.requestFrom(touchAddress, (uint8_t)1);
        if (Wire.available()) {
            uint8_t touches = Wire.read();
            return (touches > 0);
        }
    }
    return false;
}

bool TouchManager::getTouchPoint(int& x, int& y) {
    if (!initialized) return false;
    if (touchAddress == GT911_ADDR1 || touchAddress == GT911_ADDR2) {
        Wire.beginTransmission(touchAddress);
        Wire.write(0x81);
        Wire.write(0x50);
        if (Wire.endTransmission() != 0) return false;
        
        Wire.requestFrom(touchAddress, (uint8_t)4);
        if (Wire.available() >= 4) {
            uint8_t xl = Wire.read();
            uint8_t xh = Wire.read();
            uint8_t yl = Wire.read();
            uint8_t yh = Wire.read();
            x = (xh << 8) | xl;
            y = (yh << 8) | yl;
            return true;
        }
    } else if (touchAddress == CST820_ADDR) {
        Wire.beginTransmission(touchAddress);
        Wire.write(0x03);
        if (Wire.endTransmission() != 0) return false;
        
        Wire.requestFrom(touchAddress, (uint8_t)4);
        if (Wire.available() >= 4) {
            uint8_t xh = Wire.read();
            uint8_t xl = Wire.read();
            uint8_t yh = Wire.read();
            uint8_t yl = Wire.read();
            x = ((xh & 0x0F) << 8) | xl;
            y = ((yh & 0x0F) << 8) | yl;
            return true;
        }
    }
    return false;
}
#endif
