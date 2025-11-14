# Unit Tests

## Float Serialization Test

Tests ArduinoJson float serialization to ensure values like `0.0` don't get optimized to integer `0`, which causes InfluxDB field type conflicts.

### Run Tests

```bash
# Run all tests
pio test

# Run with verbose output
pio test -v

# Run specific test
pio test -f test_float_serialization
```

### What It Tests

1. **Direct Float Assignment**: `obj["value"] = 0.0f`
2. **Double Cast**: `obj["value"] = static_cast<double>(0.0f)`
3. **serialized() with snprintf**: `obj["value"] = serialized("0.000000")`
4. **Motor Duty Zero Scenario**: Exact simulation of TelemetryService motor_duty field
5. **InfluxDB Compatibility**: Multiple fields with zero values

### Expected Results

All tests should pass, with JSON output showing decimal points:
- ✅ `"motor_duty":0.000000` (correct)
- ❌ `"motor_duty":0` (incorrect - causes InfluxDB error)

### Troubleshooting

If tests fail, it indicates ArduinoJson is still optimizing floats to integers. The `serialized()` approach (test 3) should always pass as it bypasses type inference entirely.
