#include <unity.h>
#include "led_manager.h"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_led_manager_state(void) {
    LedManager led(4, 16, 17);
    led.begin();
    
    TEST_ASSERT_EQUAL(LedManager::STATE_BOOT, led.getState());
    
    led.setState(LedManager::STATE_OFF);
    TEST_ASSERT_EQUAL(LedManager::STATE_OFF, led.getState());
    
    led.setState(LedManager::STATE_OPTIMIZING);
    TEST_ASSERT_EQUAL(LedManager::STATE_OPTIMIZING, led.getState());
}

void test_led_manager_enabled(void) {
    LedManager led(4, 16, 17);
    led.begin();
    
    TEST_ASSERT_TRUE(led.isEnabled());
    
    led.setEnabled(false);
    TEST_ASSERT_FALSE(led.isEnabled());
}

void test_led_manager_brightness(void) {
    LedManager led(4, 16, 17);
    led.begin();
    
    TEST_ASSERT_EQUAL(255, led.getBrightness());
    
    led.setBrightness(128);
    TEST_ASSERT_EQUAL(128, led.getBrightness());
}

void test_led_manager_update(void) {
    LedManager led(4, 16, 17);
    led.begin();
    
    led.setState(LedManager::STATE_BOOT);
    led.update(0);
    led.update(500);
    
    led.setState(LedManager::STATE_OPTIMIZING);
    led.update(1000);
    led.update(1100);

    led.setState(LedManager::STATE_SLIDESHOW);
    led.update(2000);

    led.setState(LedManager::STATE_TRANSITION);
    led.update(3000);
    led.update(3500);

    led.setState(LedManager::STATE_ERROR);
    led.update(4000);

    led.setState(LedManager::STATE_SCREENSHOT);
    led.update(5000);
    led.update(5500);

    led.setState(LedManager::STATE_SETTINGS);
    led.update(6000);

    // Call update on disabled led
    led.setEnabled(false);
    led.update(7000);

    TEST_ASSERT_EQUAL(LedManager::STATE_SETTINGS, led.getState());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_led_manager_state);
    RUN_TEST(test_led_manager_enabled);
    RUN_TEST(test_led_manager_brightness);
    RUN_TEST(test_led_manager_update);
    return UNITY_END();
}
