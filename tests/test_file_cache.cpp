#include <unity.h>
#include "file_cache.h"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_file_cache_empty(void) {
    FileCache cache(5);
    TEST_ASSERT_TRUE(cache.isEmpty());
    TEST_ASSERT_EQUAL_INT(0, cache.size());
    TEST_ASSERT_EQUAL_STRING("", cache.getCurrent().c_str());
    TEST_ASSERT_EQUAL_STRING("", cache.getNext().c_str());
    TEST_ASSERT_EQUAL_STRING("", cache.getPrevious().c_str());
}

void test_file_cache_add_and_navigate(void) {
    FileCache cache(3);
    TEST_ASSERT_TRUE(cache.addFile("/sd/1.jpg"));
    TEST_ASSERT_TRUE(cache.addFile("/sd/2.jpg"));
    TEST_ASSERT_TRUE(cache.addFile("/sd/3.jpg"));
    TEST_ASSERT_FALSE(cache.addFile("/sd/4.jpg")); // exceeds capacity

    TEST_ASSERT_EQUAL_INT(3, cache.size());
    TEST_ASSERT_EQUAL_STRING("/sd/1.jpg", cache.getCurrent().c_str());

    TEST_ASSERT_EQUAL_STRING("/sd/2.jpg", cache.getNext().c_str());
    TEST_ASSERT_EQUAL_STRING("/sd/3.jpg", cache.getNext().c_str());
    TEST_ASSERT_EQUAL_STRING("/sd/1.jpg", cache.getNext().c_str()); // wrap around

    TEST_ASSERT_EQUAL_STRING("/sd/3.jpg", cache.getPrevious().c_str()); // wrap backward
    TEST_ASSERT_EQUAL_STRING("/sd/2.jpg", cache.getPrevious().c_str());
}

void test_file_cache_clear(void) {
    FileCache cache(5);
    cache.addFile("1.jpg");
    cache.clear();
    TEST_ASSERT_TRUE(cache.isEmpty());
    TEST_ASSERT_EQUAL_INT(0, cache.size());
}

void test_file_cache_index(void) {
    FileCache cache(5);
    cache.addFile("1.jpg");
    cache.addFile("2.jpg");
    TEST_ASSERT_EQUAL_INT(0, cache.getIndex());
    TEST_ASSERT_TRUE(cache.setIndex(1));
    TEST_ASSERT_EQUAL_INT(1, cache.getIndex());
    TEST_ASSERT_FALSE(cache.setIndex(2)); // out of bounds
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_file_cache_empty);
    RUN_TEST(test_file_cache_add_and_navigate);
    RUN_TEST(test_file_cache_clear);
    RUN_TEST(test_file_cache_index);
    return UNITY_END();
}
