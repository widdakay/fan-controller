/**
 * Unit test for ArduinoJson float serialization
 * Tests to ensure 0.0 doesn't serialize as integer 0
 *
 * Run with: pio test -e test
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <unity.h>

void test_serialized_snprintf_zero() {
    StaticJsonDocument<256> doc;
    JsonObject obj = doc.to<JsonObject>();

    // Test the exact approach used in TelemetryService.hpp
    char valueStr[16];
    snprintf(valueStr, sizeof(valueStr), "%.6f", 0.0f);
    obj["motor_duty"] = serialized(valueStr);

    String output;
    serializeJson(doc, output);

    Serial.println("\n=== Test: serialized() with snprintf for 0.0 ===");
    Serial.print("Output: ");
    Serial.println(output);

    // Must contain decimal point
    TEST_ASSERT_TRUE_MESSAGE(output.indexOf(".") != -1, "Output must contain decimal point");

    // Should not be just "0" without decimal
    TEST_ASSERT_FALSE_MESSAGE(output == "{\"motor_duty\":0}", "Should not be integer 0");

    // Should be "0.000000"
    TEST_ASSERT_TRUE_MESSAGE(output.indexOf("0.000000") != -1, "Should contain 0.000000");
}

void test_serialized_snprintf_nonzero() {
    StaticJsonDocument<256> doc;
    JsonObject obj = doc.to<JsonObject>();

    char valueStr[16];
    snprintf(valueStr, sizeof(valueStr), "%.6f", 0.5f);
    obj["motor_duty"] = serialized(valueStr);

    String output;
    serializeJson(doc, output);

    Serial.println("\n=== Test: serialized() with snprintf for 0.5 ===");
    Serial.print("Output: ");
    Serial.println(output);

    TEST_ASSERT_TRUE_MESSAGE(output.indexOf("0.500000") != -1, "Should contain 0.500000");
}

void test_double_cast_zero() {
    StaticJsonDocument<256> doc;
    JsonObject obj = doc.to<JsonObject>();

    obj["value"] = static_cast<double>(0.0f);

    String output;
    serializeJson(doc, output);

    Serial.println("\n=== Test: double cast for 0.0 ===");
    Serial.print("Output: ");
    Serial.println(output);

    // This might fail - double cast doesn't always work
    bool hasDecimal = output.indexOf(".") != -1;
    if (!hasDecimal) {
        Serial.println("WARNING: double cast doesn't preserve decimal for 0.0");
    }
    // Don't fail test, just warn
    TEST_ASSERT_TRUE(true);
}

void test_direct_assignment_zero() {
    StaticJsonDocument<256> doc;
    JsonObject obj = doc.to<JsonObject>();

    obj["value"] = 0.0f;

    String output;
    serializeJson(doc, output);

    Serial.println("\n=== Test: direct assignment of 0.0f ===");
    Serial.print("Output: ");
    Serial.println(output);

    // This will likely fail - ArduinoJson optimizes to int
    bool hasDecimal = output.indexOf(".") != -1;
    if (!hasDecimal) {
        Serial.println("EXPECTED: Direct assignment optimizes 0.0 to integer 0");
    }
    // Don't fail test, just demonstrate the problem
    TEST_ASSERT_TRUE(true);
}

void test_motor_duty_scenario() {
    // Simulate exact TelemetryService scenario
    StaticJsonDocument<512> doc;
    JsonObject fields = doc.to<JsonObject>();

    float dutyCycle = 0.0f;  // Motor off

    // Use serialized() approach like in TelemetryService.hpp:61-63
    char dutyStr[16];
    snprintf(dutyStr, sizeof(dutyStr), "%.6f", dutyCycle);
    fields["motor_duty"] = serialized(dutyStr);

    fields["motor_direction"] = 0;
    fields["motor_en_a"] = 1;
    fields["motor_en_b"] = 1;
    fields["motor_fault"] = 0;

    String output;
    serializeJson(doc, output);

    Serial.println("\n=== Test: Full motor status with duty=0.0 ===");
    Serial.print("Output: ");
    Serial.println(output);

    // Verify motor_duty has decimal
    int motorDutyPos = output.indexOf("motor_duty");
    TEST_ASSERT_TRUE_MESSAGE(motorDutyPos != -1, "motor_duty field should exist");

    // Extract the value after motor_duty
    int colonPos = output.indexOf(":", motorDutyPos);
    int commaPos = output.indexOf(",", colonPos);
    if (commaPos == -1) commaPos = output.indexOf("}", colonPos);

    String valueStr = output.substring(colonPos + 1, commaPos);
    valueStr.trim();

    Serial.print("motor_duty value: ");
    Serial.println(valueStr);

    // Must contain decimal point
    TEST_ASSERT_TRUE_MESSAGE(valueStr.indexOf(".") != -1, "motor_duty must have decimal point");
}

void test_comparison_all_methods() {
    Serial.println("\n\n===================================================");
    Serial.println("Comparison of all serialization methods for 0.0:");
    Serial.println("===================================================\n");

    // Method 1: Direct assignment
    {
        StaticJsonDocument<128> doc;
        JsonObject obj = doc.to<JsonObject>();
        obj["value"] = 0.0f;
        String output;
        serializeJson(doc, output);
        Serial.print("1. Direct assignment (0.0f):        ");
        Serial.println(output);
    }

    // Method 2: Double cast
    {
        StaticJsonDocument<128> doc;
        JsonObject obj = doc.to<JsonObject>();
        obj["value"] = static_cast<double>(0.0f);
        String output;
        serializeJson(doc, output);
        Serial.print("2. Double cast:                      ");
        Serial.println(output);
    }

    // Method 3: Adding 0.0f
    {
        StaticJsonDocument<128> doc;
        JsonObject obj = doc.to<JsonObject>();
        obj["value"] = 0.0f + 0.0f;
        String output;
        serializeJson(doc, output);
        Serial.print("3. Add 0.0f:                         ");
        Serial.println(output);
    }

    // Method 4: serialized() with snprintf
    {
        StaticJsonDocument<128> doc;
        JsonObject obj = doc.to<JsonObject>();
        char str[16];
        snprintf(str, sizeof(str), "%.6f", 0.0f);
        obj["value"] = serialized(str);
        String output;
        serializeJson(doc, output);
        Serial.print("4. serialized(snprintf(%.6f)):       ");
        Serial.println(output);
    }

    Serial.println("\n===================================================");
    Serial.println("✓ Method 4 (serialized + snprintf) is the solution");
    Serial.println("===================================================\n");

    TEST_ASSERT_TRUE(true);
}

void setup() {
    delay(2000);  // Wait for serial
    Serial.begin(115200);
    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.println("ArduinoJson Float Serialization Tests");
    Serial.println("========================================");

    UNITY_BEGIN();

    // Test the solution
    RUN_TEST(test_serialized_snprintf_zero);
    RUN_TEST(test_serialized_snprintf_nonzero);
    RUN_TEST(test_motor_duty_scenario);

    // Test other methods (for comparison)
    RUN_TEST(test_double_cast_zero);
    RUN_TEST(test_direct_assignment_zero);

    // Summary comparison
    RUN_TEST(test_comparison_all_methods);

    UNITY_END();

    Serial.println("\n========================================");
    Serial.println("All tests complete!");
    Serial.println("========================================\n");
}

void loop() {
    // Tests run once in setup()
}
