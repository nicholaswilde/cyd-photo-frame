#include <unity.h>
#include "lvgl_manager.h"

void setUp(void) {}
void tearDown(void) {}

void test_lvgl_manager_initial_state(void) {
    TEST_ASSERT_FALSE(LVGLManager::isInitialized());
}

void test_lvgl_manager_init(void) {
    LVGLManager::init(320, 240);
    TEST_ASSERT_TRUE(LVGLManager::isInitialized());
    TEST_ASSERT_EQUAL_INT(320, LVGLManager::getWidth());
    TEST_ASSERT_EQUAL_INT(240, LVGLManager::getHeight());
}

static void dummy_cb() {}

void test_lvgl_manager_callbacks_and_screens(void) {
    LVGLManager::init(320, 240);
    LVGLManager::setExitCallback(dummy_cb);
    LVGLManager::setRebootConfirmCallback(dummy_cb);
    LVGLManager::setCancelCallback(dummy_cb);
    LVGLManager::setClearCacheCallback(dummy_cb);
    
    LVGLManager::handle();
    
    // Assert initialization to ensure it didn't crash
    TEST_ASSERT_TRUE(LVGLManager::isInitialized());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_lvgl_manager_initial_state);
    RUN_TEST(test_lvgl_manager_init);
    RUN_TEST(test_lvgl_manager_callbacks_and_screens);
    return UNITY_END();
}
