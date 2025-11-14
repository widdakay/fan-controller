// Test program to verify ArduinoJson float serialization
// Compile with: g++ -std=c++17 test_json_float.cpp -I~/.platformio/packages/framework-arduinoespressif32/libraries/ArduinoJson/src -o test_json && ./test_json

#include <iostream>
#include <sstream>
#include <cstdio>
#include <ArduinoJson.h>

// Mock String class for compatibility
class String {
public:
    std::string data;
    String() {}
    String(const char* s) : data(s) {}
    String(int n) : data(std::to_string(n)) {}
    String(double d) : data(std::to_string(d)) {}
    const char* c_str() const { return data.c_str(); }
    size_t length() const { return data.length(); }
};

void test_different_approaches() {
    StaticJsonDocument<512> doc;
    JsonObject fields = doc.to<JsonObject>();

    float zeroValue = 0.0f;
    float normalValue = 0.5f;

    std::cout << "=== Testing Different Float Serialization Approaches ===" << std::endl;

    // Test 1: Direct assignment
    doc.clear();
    fields = doc.to<JsonObject>();
    fields["direct_zero"] = zeroValue;
    fields["direct_normal"] = normalValue;

    String output1;
    serializeJson(doc, output1.data);
    std::cout << "1. Direct assignment: " << output1.c_str() << std::endl;

    // Test 2: Cast to double
    doc.clear();
    fields = doc.to<JsonObject>();
    fields["double_zero"] = static_cast<double>(zeroValue);
    fields["double_normal"] = static_cast<double>(normalValue);

    String output2;
    serializeJson(doc, output2.data);
    std::cout << "2. Cast to double: " << output2.c_str() << std::endl;

    // Test 3: Add epsilon
    doc.clear();
    fields = doc.to<JsonObject>();
    fields["epsilon_zero"] = zeroValue + 0.0f;
    fields["epsilon_normal"] = normalValue + 0.0f;

    String output3;
    serializeJson(doc, output3.data);
    std::cout << "3. Add epsilon (+0.0f): " << output3.c_str() << std::endl;

    // Test 4: Using serialized() with snprintf
    doc.clear();
    fields = doc.to<JsonObject>();
    char zeroStr[16], normalStr[16];
    snprintf(zeroStr, sizeof(zeroStr), "%.6f", zeroValue);
    snprintf(normalStr, sizeof(normalStr), "%.6f", normalValue);
    fields["serialized_zero"] = serialized(zeroStr);
    fields["serialized_normal"] = serialized(normalStr);

    String output4;
    serializeJson(doc, output4.data);
    std::cout << "4. Using serialized(snprintf): " << output4.c_str() << std::endl;

    std::cout << "\n=== Expected for InfluxDB Compatibility ===" << std::endl;
    std::cout << "All values should have decimal points (e.g., 0.0 not 0)" << std::endl;
}

int main() {
    test_different_approaches();
    return 0;
}
