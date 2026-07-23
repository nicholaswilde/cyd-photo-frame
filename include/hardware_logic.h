#ifndef HARDWARE_LOGIC_H
#define HARDWARE_LOGIC_H

#include <stdint.h>
#include <string>

#if defined(NATIVE_TEST)
#include "mocks/Preferences.h"
#else
#include <Preferences.h>
#endif

namespace HardwareLogic {
    // Map LDR analog reading (0-4095) to brightness (25-255)
    int mapLdrToBrightness(int ldrValue);

    // Apply exponential smoothing filter to brightness
    int smoothBrightness(int current, int target, float alpha = 0.1f);

    // Load settings from Preferences
    void loadSettings(Preferences& prefs, 
                      int& brightness, 
                      bool& autoBright, 
                      unsigned long& delay, 
                      bool& randomMode, 
                      bool& showFilename, 
                      bool& inactivitySleep,
                      int& themeFlavor,
                      int& screenOrientation,
                      int& ledBrightness,
                      bool& isLedEnabled,
                      bool& isWifiEnabled,
                      bool& isMqttEnabled,
                      std::string& wifiSSID,
                      std::string& wifiPassword,
                      bool& bypassOptimization);

    // Save settings to Preferences
    void saveSettings(Preferences& prefs, 
                      int brightness, 
                      bool autoBright, 
                      unsigned long delay, 
                      bool randomMode, 
                      bool showFilename, 
                      bool inactivitySleep,
                      int themeFlavor,
                      int screenOrientation,
                      int ledBrightness,
                      bool isLedEnabled,
                      bool isWifiEnabled,
                      bool isMqttEnabled,
                      const std::string& wifiSSID,
                      const std::string& wifiPassword,
                      bool bypassOptimization);
}

#endif // HARDWARE_LOGIC_H
