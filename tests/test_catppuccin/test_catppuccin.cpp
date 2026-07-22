#include <unity.h>
#include "catppuccin.h"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_catppuccin_mocha(void) {
    const CatppuccinColors& colors = getCatppuccinFlavor(CATPPUCCIN_MOCHA);
    TEST_ASSERT_EQUAL_HEX32(0x1e1e2e, colors.base);
    TEST_ASSERT_EQUAL_HEX32(0x181825, colors.mantle);
    TEST_ASSERT_EQUAL_HEX32(0x11111b, colors.crust);
}

void test_catppuccin_macchiato(void) {
    const CatppuccinColors& colors = getCatppuccinFlavor(CATPPUCCIN_MACCHIATO);
    TEST_ASSERT_EQUAL_HEX32(0x24273a, colors.base);
}

void test_catppuccin_frappe(void) {
    const CatppuccinColors& colors = getCatppuccinFlavor(CATPPUCCIN_FRAPPE);
    TEST_ASSERT_EQUAL_HEX32(0x303446, colors.base);
}

void test_catppuccin_latte(void) {
    const CatppuccinColors& colors = getCatppuccinFlavor(CATPPUCCIN_LATTE);
    TEST_ASSERT_EQUAL_HEX32(0xeff1f5, colors.base);
}

void test_catppuccin_default(void) {
    const CatppuccinColors& colors = getCatppuccinFlavor(999);
    TEST_ASSERT_EQUAL_HEX32(0x1e1e2e, colors.base);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_catppuccin_mocha);
    RUN_TEST(test_catppuccin_macchiato);
    RUN_TEST(test_catppuccin_frappe);
    RUN_TEST(test_catppuccin_latte);
    RUN_TEST(test_catppuccin_default);
    return UNITY_END();
}
