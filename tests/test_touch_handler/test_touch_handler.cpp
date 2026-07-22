#include <unity.h>
#include "touch_handler.h"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_touch_handler_mapping_capacitive(void) {
    TouchHandler handler(480, 320); // CYD-35C (orientation 1 landscape by default)
    int px = -1, py = -1;
    // Native portrait touch (160, 240) on 480x320 screen in landscape (orientation 1):
    // pixelX = 240, pixelY = 320 - 160 = 160
    handler.mapCoordinates(160, 240, px, py, true);
    TEST_ASSERT_EQUAL_INT(240, px);
    TEST_ASSERT_EQUAL_INT(160, py);
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
    
    // Middle Left Zone (raw 500, 2000 maps to pixel X=26, Y=120)
    TEST_ASSERT_EQUAL(TouchZone::MID_LEFT, handler.processTouch(true, 500, 2000, 1000));
    
    // Middle Center Zone (raw 2000, 2000 maps to pixel X=160, Y=120)
    // Debounce should prevent trigger at same time
    TEST_ASSERT_EQUAL(TouchZone::NONE, handler.processTouch(true, 2000, 2000, 1000));
    // After debounce interval
    TEST_ASSERT_EQUAL(TouchZone::MID_CENTER_DOWN, handler.processTouch(true, 2000, 2000, 1400));
    
    // Middle Right Zone (raw 3500, 2000 maps to pixel X=293, Y=120)
    TEST_ASSERT_EQUAL(TouchZone::MID_RIGHT, handler.processTouch(true, 3500, 2000, 1800));
    
    // Top Left Zone (raw 500, 500 maps to pixel X=26, Y=20 - which is top 1/4)
    TEST_ASSERT_EQUAL(TouchZone::TOP_LEFT, handler.processTouch(true, 500, 500, 2200));
    
    // Bottom Center Zone (raw 2000, 3500 maps to pixel X=160, Y=220 - which is bottom 1/4)
    TEST_ASSERT_EQUAL(TouchZone::BOTTOM_CENTER, handler.processTouch(true, 2000, 3500, 2600));
}

void test_touch_handler_no_touch(void) {
    TouchHandler handler(320, 240);
    TEST_ASSERT_EQUAL(TouchZone::NONE, handler.processTouch(false, 2000, 2000, 1000));
}

void test_touch_handler_long_press(void) {
    TouchHandler handler(320, 240);
    
    // Start touch at T=1000ms
    TEST_ASSERT_EQUAL(TouchZone::MID_CENTER_DOWN, handler.processTouch(true, 2000, 2000, 1000));
    
    // Hold touch at T=1500ms (500ms elapsed) - should return NONE
    TEST_ASSERT_EQUAL(TouchZone::NONE, handler.processTouch(true, 2000, 2000, 1500));
    
    // Hold touch at T=2490ms (1490ms elapsed) - should return NONE
    TEST_ASSERT_EQUAL(TouchZone::NONE, handler.processTouch(true, 2000, 2000, 2490));
    
    // Hold touch at T=2500ms (1500ms elapsed) - should return LONG_PRESS_MID_CENTER
    TEST_ASSERT_EQUAL(TouchZone::LONG_PRESS_MID_CENTER, handler.processTouch(true, 2000, 2000, 2500));
    
    // Continue holding at T=2600ms - should return NONE (already triggered)
    TEST_ASSERT_EQUAL(TouchZone::NONE, handler.processTouch(true, 2000, 2000, 2600));
    
    // Release touch at T=3000ms
    TEST_ASSERT_EQUAL(TouchZone::NONE, handler.processTouch(false, 2000, 2000, 3000));
}

void test_touch_handler_portrait_zones(void) {
    TouchHandler handler(320, 240);
    handler.setOrientation(2); // Portrait mode
    
    // Touch in physical middle of portrait screen (raw 2000, 2000)
    TEST_ASSERT_EQUAL(TouchZone::MID_CENTER_DOWN, handler.processTouch(true, 2000, 2000, 1000));
    
    // Hold long press at T=2500ms in portrait mode
    TEST_ASSERT_EQUAL(TouchZone::LONG_PRESS_MID_CENTER, handler.processTouch(true, 2000, 2000, 2500));
}

void test_touch_handler_portrait_resistive_mapping(void) {
    TouchHandler handler(240, 320); // Portrait width=240, height=320
    int px = -1, py = -1;
    
    // Orientation 2 (Portrait):
    // px = ly, py = 320 - lx
    handler.setOrientation(2);
    // rawX = 2000 maps to lx = 160, rawY = 1100 maps to ly = 60
    handler.mapCoordinates(2000, 1100, px, py, false);
    TEST_ASSERT_INT_WITHIN(5, 60, px);
    TEST_ASSERT_INT_WITHIN(5, 160, py);
    
    // Orientation 0 (Portrait Rev):
    // px = 240 - ly, py = lx
    handler.setOrientation(0);
    handler.mapCoordinates(2000, 1100, px, py, false);
    TEST_ASSERT_INT_WITHIN(5, 180, px);
    TEST_ASSERT_INT_WITHIN(5, 160, py);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_touch_handler_mapping_capacitive);
    RUN_TEST(test_touch_handler_mapping_resistive);
    RUN_TEST(test_touch_handler_zones);
    RUN_TEST(test_touch_handler_no_touch);
    RUN_TEST(test_touch_handler_long_press);
    RUN_TEST(test_touch_handler_portrait_zones);
    RUN_TEST(test_touch_handler_portrait_resistive_mapping);
    return UNITY_END();
}
