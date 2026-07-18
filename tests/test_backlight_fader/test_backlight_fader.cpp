#include <unity.h>
#include "backlight_fader.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_fader_idle_initially(void) {
    BacklightFader fader;
    TEST_ASSERT_EQUAL(BacklightFader::STATE_IDLE, fader.getState());
    
    int current = 128;
    bool finished = fader.update(1000, current);
    TEST_ASSERT_FALSE(finished);
    TEST_ASSERT_EQUAL_INT(0, current); // target is 0 by default
}

void test_fader_interpolation(void) {
    BacklightFader fader;
    fader.startFade(255, 0, 1000); // Fade out from 255 to 0 over 1000ms
    TEST_ASSERT_EQUAL(BacklightFader::STATE_FADE_OUT, fader.getState());

    int current = 255;
    // First update registers start time
    bool finished = fader.update(1000, current);
    TEST_ASSERT_FALSE(finished);
    TEST_ASSERT_EQUAL_INT(255, current); // t=0

    // Update at t=500ms (50% progress)
    finished = fader.update(1500, current);
    TEST_ASSERT_FALSE(finished);
    TEST_ASSERT_EQUAL_INT(128, current); // 255 + (0-255)*0.5 = 255 - 127 = 128

    // Update at t=1000ms (100% progress)
    finished = fader.update(2000, current);
    TEST_ASSERT_TRUE(finished);
    TEST_ASSERT_EQUAL_INT(0, current);
    TEST_ASSERT_EQUAL(BacklightFader::STATE_IDLE, fader.getState());
}

void test_fader_fade_in(void) {
    BacklightFader fader;
    fader.startFade(0, 200, 500); // Fade in from 0 to 200 over 500ms
    TEST_ASSERT_EQUAL(BacklightFader::STATE_FADE_IN, fader.getState());

    int current = 0;
    // First update registers start time
    bool finished = fader.update(1000, current);
    TEST_ASSERT_FALSE(finished);
    TEST_ASSERT_EQUAL_INT(0, current);

    // Update at 250ms (50% progress)
    finished = fader.update(1250, current);
    TEST_ASSERT_FALSE(finished);
    TEST_ASSERT_EQUAL_INT(100, current);

    // Update at 600ms (overshoot)
    finished = fader.update(1600, current);
    TEST_ASSERT_TRUE(finished);
    TEST_ASSERT_EQUAL_INT(200, current);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_fader_idle_initially);
    RUN_TEST(test_fader_interpolation);
    RUN_TEST(test_fader_fade_in);
    return UNITY_END();
}
