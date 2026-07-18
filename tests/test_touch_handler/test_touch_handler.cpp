#include <unity.h>
#include "touch_handler.h"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_touch_handler_mapping_capacitive(void) {
    TouchHandler handler(480, 320); // CYD-35C
    int px = -1, py = -1;
    handler.mapCoordinates(150, 200, px, py, true);
    TEST_ASSERT_EQUAL_INT(150, px);
    TEST_ASSERT_EQUAL_INT(200, py);
}

void test_touch_handler_mapping_resistive(void) {
    TouchHandler handler(320, 240); // CYD-28R
    int px = -1, py = -1;
    // Map middle (raw around 2000)
    handler.mapCoordinates(2000, 2000, px, py, false);
    TEST_ASSERT_INT_WITHIN(20, 160, px);
    TEST_ASSERT_INT_WITHIN(20, 120, py);
}

void test_touch_handler_zones(void) {
    TouchHandler handler(320, 240);
    
    // Left Zone (0 - 96 pixels)
    TEST_ASSERT_EQUAL(TouchZone::LEFT, handler.processTouch(true, 500, 2000, 1000));
    
    // Center Zone (96 - 224 pixels)
    // Debounce should prevent trigger at same time
    TEST_ASSERT_EQUAL(TouchZone::NONE, handler.processTouch(true, 2000, 2000, 1000));
    // After debounce interval
    TEST_ASSERT_EQUAL(TouchZone::CENTER, handler.processTouch(true, 2000, 2000, 1400));
    
    // Right Zone (224 - 320 pixels)
    TEST_ASSERT_EQUAL(TouchZone::RIGHT, handler.processTouch(true, 3500, 2000, 1800));
}

void test_touch_handler_no_touch(void) {
    TouchHandler handler(320, 240);
    TEST_ASSERT_EQUAL(TouchZone::NONE, handler.processTouch(false, 2000, 2000, 1000));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_touch_handler_mapping_capacitive);
    RUN_TEST(test_touch_handler_mapping_resistive);
    RUN_TEST(test_touch_handler_zones);
    RUN_TEST(test_touch_handler_no_touch);
    return UNITY_END();
}
