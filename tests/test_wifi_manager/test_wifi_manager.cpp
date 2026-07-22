#include <unity.h>
#include "wifi_manager.h"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_wifi_manager_stub(void) {
    WifiManager wm("myssid", "mypass");
    wm.begin(); // Should do nothing in native test except set state
    wm.update(); // Should do nothing
    
    TEST_ASSERT_EQUAL(WIFI_STATE_CONNECTED, wm.getState());
    TEST_ASSERT_EQUAL_STRING("127.0.0.1", wm.getIPAddress().c_str());
    TEST_ASSERT_EQUAL_STRING("cyd-photo-frame-mock", wm.getAPSSID().c_str());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_wifi_manager_stub);
    return UNITY_END();
}
