#include <unity.h>
#include "hardware_logic.h"

void setUp(void) {
    Preferences prefs;
    prefs.begin("settings");
    prefs.clear();
}

void tearDown(void) {
}

void test_ldr_mapping(void) {
    // Test mapping of 0-4095 value to 25-255 brightness
    TEST_ASSERT_EQUAL_INT(25, HardwareLogic::mapLdrToBrightness(0));
    TEST_ASSERT_EQUAL_INT(25, HardwareLogic::mapLdrToBrightness(-100)); // clamp low
    TEST_ASSERT_EQUAL_INT(255, HardwareLogic::mapLdrToBrightness(4095));
    TEST_ASSERT_EQUAL_INT(255, HardwareLogic::mapLdrToBrightness(5000)); // clamp high
    
    // Test midpoints
    TEST_ASSERT_EQUAL_INT(140, HardwareLogic::mapLdrToBrightness(2048)); // 25 + 2048 * 230 / 4095 = 25 + 115 = 140
    TEST_ASSERT_EQUAL_INT(53, HardwareLogic::mapLdrToBrightness(500));
}

void test_brightness_smoothing(void) {
    // Test smoothing filter
    // current=100, target=200, alpha=0.1 -> 100 * 0.9 + 200 * 0.1 = 90 + 20 = 110
    TEST_ASSERT_EQUAL_INT(110, HardwareLogic::smoothBrightness(100, 200, 0.1f));
    
    // Test alpha bounds
    TEST_ASSERT_EQUAL_INT(100, HardwareLogic::smoothBrightness(100, 200, -0.5f)); // clamp alpha low (no change)
    TEST_ASSERT_EQUAL_INT(200, HardwareLogic::smoothBrightness(100, 200, 1.5f));  // clamp alpha high (instant change)
    
    // Test clamping brightness boundaries
    TEST_ASSERT_EQUAL_INT(25, HardwareLogic::smoothBrightness(25, 0, 0.5f)); // min clamp
    TEST_ASSERT_EQUAL_INT(255, HardwareLogic::smoothBrightness(255, 300, 0.5f)); // max clamp
}

void test_preferences_persistence(void) {
    Preferences prefs;
    prefs.begin("settings");
    
    // Initial values
    int brightness = 128;
    bool autoBright = true;
    unsigned long delay = 15000;
    bool randomMode = false;
    bool showFilename = true;
    bool inactivitySleep = false;
    int themeFlavor = 1; // CATPPUCCIN_MOCHA
    int screenOrientation = 1; // Landscape
    int ledBrightness = 200;
    
    // Save settings
    HardwareLogic::saveSettings(prefs, brightness, autoBright, delay, randomMode, showFilename, inactivitySleep, themeFlavor, screenOrientation, ledBrightness);
    
    // Modify local variables to verify they load correctly
    int testBrightness = 0;
    bool testAutoBright = false;
    unsigned long testDelay = 0;
    bool testRandomMode = true;
    bool testShowFilename = false;
    bool testInactivitySleep = true;
    int testThemeFlavor = 0;
    int testScreenOrientation = 0;
    int testLedBrightness = 0;
    
    // Load settings
    HardwareLogic::loadSettings(prefs, testBrightness, testAutoBright, testDelay, testRandomMode, testShowFilename, testInactivitySleep, testThemeFlavor, testScreenOrientation, testLedBrightness);
    
    // Verify loaded values match saved values
    TEST_ASSERT_EQUAL_INT(brightness, testBrightness);
    TEST_ASSERT_EQUAL(autoBright, testAutoBright);
    TEST_ASSERT_EQUAL_UINT32(delay, testDelay);
    TEST_ASSERT_EQUAL(randomMode, testRandomMode);
    TEST_ASSERT_EQUAL(showFilename, testShowFilename);
    TEST_ASSERT_EQUAL(inactivitySleep, testInactivitySleep);
    TEST_ASSERT_EQUAL_INT(themeFlavor, testThemeFlavor);
    TEST_ASSERT_EQUAL_INT(screenOrientation, testScreenOrientation);
    TEST_ASSERT_EQUAL_INT(ledBrightness, testLedBrightness);
    
    // Verify default fallback works when loading non-existent settings
    Preferences freshPrefs;
    freshPrefs.begin("empty");
    
    int fallbackBrightness = 200;
    bool fallbackAutoBright = true;
    unsigned long fallbackDelay = 10000;
    bool fallbackRandom = false;
    bool fallbackShowFn = true;
    bool fallbackSleep = false;
    int fallbackTheme = 1;
    int fallbackOrientation = 2;
    int fallbackLedBrightness = 128;
    
    HardwareLogic::loadSettings(freshPrefs, fallbackBrightness, fallbackAutoBright, fallbackDelay, fallbackRandom, fallbackShowFn, fallbackSleep, fallbackTheme, fallbackOrientation, fallbackLedBrightness);
    
    TEST_ASSERT_EQUAL_INT(200, fallbackBrightness);
    TEST_ASSERT_EQUAL(true, fallbackAutoBright);
    TEST_ASSERT_EQUAL_UINT32(10000, fallbackDelay);
    TEST_ASSERT_EQUAL_INT(1, fallbackTheme);
    TEST_ASSERT_EQUAL_INT(2, fallbackOrientation);
    TEST_ASSERT_EQUAL_INT(128, fallbackLedBrightness);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_ldr_mapping);
    RUN_TEST(test_brightness_smoothing);
    RUN_TEST(test_preferences_persistence);
    return UNITY_END();
}
