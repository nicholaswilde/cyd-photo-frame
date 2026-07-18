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

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_lvgl_manager_initial_state);
    RUN_TEST(test_lvgl_manager_init);
    return UNITY_END();
}
