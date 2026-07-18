#include <unity.h>
#include "touch_manager.h"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_touch_manager_mock_default(void) {
    TouchManager::begin();
    TEST_ASSERT_FALSE(TouchManager::isTouched());
    int x = -1, y = -1;
    TEST_ASSERT_FALSE(TouchManager::getTouchPoint(x, y));
}

void test_touch_manager_mock_set(void) {
    setMockTouch(true, 150, 200);
    TEST_ASSERT_TRUE(TouchManager::isTouched());
    int x = -1, y = -1;
    TEST_ASSERT_TRUE(TouchManager::getTouchPoint(x, y));
    TEST_ASSERT_EQUAL_INT(150, x);
    TEST_ASSERT_EQUAL_INT(200, y);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_touch_manager_mock_default);
    RUN_TEST(test_touch_manager_mock_set);
    return UNITY_END();
}
