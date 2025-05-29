#include <Arduino.h>
#include <unity.h>
#include "temp_logic.h"

void test_temp_over() {
    std::string result1 = isOverThreshold(31.0, 30.0);
    std::string result2 = isOverThreshold(28.5, 30.0);
    TEST_ASSERT_NOT_EQUAL(-1, result1.find("Vượt ngưỡng"));
    TEST_ASSERT_NOT_EQUAL(-1, result2.find("Không vượt ngưỡng"));
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_temp_over);
    UNITY_END();
}
void loop() {}