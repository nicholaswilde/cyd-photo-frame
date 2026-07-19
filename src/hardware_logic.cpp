#include "hardware_logic.h"

namespace HardwareLogic {

int mapLdrToBrightness(int ldrValue) {
    if (ldrValue < 0) ldrValue = 0;
    if (ldrValue > 4095) ldrValue = 4095;
    
    // Map 0-4095 LDR reading to 25-255 brightness duty cycle
    // Values close to 0 (dark) -> 25 (minimum visible backlight)
    // Values close to 4095 (bright) -> 255 (maximum backlight)
    int mapped = 25 + (ldrValue * 230 / 4095);
    return mapped;
}

int smoothBrightness(int current, int target, float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    float currentF = (float)current;
    float targetF = (float)target;
    float nextF = (currentF * (1.0f - alpha)) + (targetF * alpha);
    
    int next = (int)(nextF + 0.5f);
    if (next < 25) next = 25;
    if (next > 255) next = 255;
    return next;
}

void loadSettings(Preferences& prefs, 
                  int& brightness, 
                  bool& autoBright, 
                  unsigned long& delay, 
                  bool& randomMode, 
                  bool& showFilename, 
                  bool& inactivitySleep,
                  int& themeFlavor) {
    brightness = prefs.getUChar("bright", brightness);
    autoBright = prefs.getBool("autob", autoBright);
    delay = prefs.getULong("delay", delay);
    randomMode = prefs.getBool("random", randomMode);
    showFilename = prefs.getBool("showfn", showFilename);
    inactivitySleep = prefs.getBool("inacts", inactivitySleep);
    themeFlavor = (int)prefs.getUInt("theme", (uint32_t)themeFlavor);
}

void saveSettings(Preferences& prefs, 
                  int brightness, 
                  bool autoBright, 
                  unsigned long delay, 
                  bool randomMode, 
                  bool showFilename, 
                  bool inactivitySleep,
                  int themeFlavor) {
    prefs.putUChar("bright", (uint8_t)brightness);
    prefs.putBool("autob", autoBright);
    prefs.putULong("delay", (uint32_t)delay);
    prefs.putBool("random", randomMode);
    prefs.putBool("showfn", showFilename);
    prefs.putBool("inacts", inactivitySleep);
    prefs.putUInt("theme", (uint32_t)themeFlavor);
}

} // namespace HardwareLogic
