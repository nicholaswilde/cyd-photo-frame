#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <stdint.h>

#define OUTPUT 0x01
#define INPUT 0x00
#define HIGH 0x01
#define LOW  0x00

inline void pinMode(int pin, int mode) {}
inline void digitalWrite(int pin, int val) {}
inline unsigned long millis() { return 0; }

#endif
