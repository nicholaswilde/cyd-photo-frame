#include <unity.h>
#include "slideshow_timer.h"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_timer_not_elapsed_initially(void) {
    SlideshowTimer timer(10000);
    timer.start(1000);
    TEST_ASSERT_FALSE(timer.isElapsed(1000));
    TEST_ASSERT_FALSE(timer.isElapsed(5000));
    TEST_ASSERT_FALSE(timer.isElapsed(10999));
}

void test_timer_elapsed_after_time(void) {
    SlideshowTimer timer(10000);
    timer.start(1000);
    TEST_ASSERT_TRUE(timer.isElapsed(11000));
    TEST_ASSERT_TRUE(timer.isElapsed(20000));
}

void test_timer_force_elapsed(void) {
    SlideshowTimer timer(10000);
    timer.start(1000);
    TEST_ASSERT_FALSE(timer.isElapsed(5000));
    timer.forceElapsed();
    TEST_ASSERT_TRUE(timer.isElapsed(5000));
}

void test_timer_paused(void) {
    SlideshowTimer timer(10000);
    timer.start(1000);
    TEST_ASSERT_FALSE(timer.isElapsed(5000));
    
    timer.setPaused(true);
    TEST_ASSERT_TRUE(timer.isPaused());
    // Should not elapse even if time passes when paused
    TEST_ASSERT_FALSE(timer.isElapsed(12000));
    
    timer.setPaused(false);
    TEST_ASSERT_FALSE(timer.isPaused());
    // Since we unpaused, we start a new interval or adjust.
    // If we call start or reset on resume, it's clean.
    timer.reset(12000);
    TEST_ASSERT_FALSE(timer.isElapsed(15000));
    TEST_ASSERT_TRUE(timer.isElapsed(22000));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_timer_not_elapsed_initially);
    RUN_TEST(test_timer_elapsed_after_time);
    RUN_TEST(test_timer_force_elapsed);
    RUN_TEST(test_timer_paused);
    return UNITY_END();
}
