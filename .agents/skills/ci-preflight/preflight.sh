#!/bin/bash

# CI Pre-Flight Check Script
# This script ensures that any code changes do not break compilation
# for the supported environments and pass all host-native unit tests.

# Exit immediately if a command exits with a non-zero status.
set -e

echo "========================================================"
echo "🚀 Starting Local CI Pre-Flight Checks"
echo "========================================================"

echo -e "\n⏳ [1/4] Building firmware for 'cyd_28r'..."
pio run -e cyd_28r
echo "✅ cyd_28r build successful!"

echo -e "\n⏳ [2/4] Building firmware for 'cyd_35c'..."
pio run -e cyd_35c
echo "✅ cyd_35c build successful!"

echo -e "\n⏳ [3/4] Running host-native unit tests..."
pio test -e native
echo "✅ Unit tests passed successfully!"

echo -e "\n⏳ [4/4] Running MQTT integration tests..."
bash .agents/skills/mqtt-testing/test_mqtt.sh
echo "✅ MQTT integration tests passed successfully!"

echo -e "\n========================================================"
echo "🎉 SUCCESS: All Pre-Flight Checks Passed!"
echo "========================================================"
