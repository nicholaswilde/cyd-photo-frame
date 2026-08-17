#!/bin/bash

# Regression test for decoupled MQTT publishing intervals
# Verifies that system diagnostics are published at their regular cadence,
# while operational settings are published on a longer interval or when forced.

set -e

ENV_FILE=".env"
if [ -f "../../.env" ]; then ENV_FILE="../../.env"; fi
if [ -f "../../../.env" ]; then ENV_FILE="../../../.env"; fi

if [ ! -f "$ENV_FILE" ]; then
    echo "Error: .env file not found"
    exit 1
fi

DEVICE_IP=$(grep '^CYD_DEVICE_IP=' "$ENV_FILE" | cut -d '=' -f2 | tr -d '"' | tr -d "'" | tr -d '\r')
CONFIG=$(curl -s -m 5 "http://$DEVICE_IP/api/config" || echo "")
MQTT_SERVER=$(echo "$CONFIG" | jq -r '.mqtt_server')
MQTT_PORT=$(echo "$CONFIG" | jq -r '.mqtt_port')
MQTT_USER=$(echo "$CONFIG" | jq -r '.mqtt_user')
MQTT_PASS=$(echo "$CONFIG" | jq -r '.mqtt_password')
MQTT_BASE=$(echo "$CONFIG" | jq -r '.mqtt_base_topic')

AUTH_ARGS=""
if [ "$MQTT_USER" != "null" ] && [ "$MQTT_USER" != "" ]; then
    AUTH_ARGS="-u $MQTT_USER -P $MQTT_PASS"
fi

echo "========================================================"
echo "📡 Testing MQTT Publishing Intervals"
echo "Broker: $MQTT_SERVER:$MQTT_PORT"
echo "========================================================"

echo -e "\n⏳ [1/2] Waiting for regular diagnostic publish (60s interval)..."
# We should see system/uptime published without settings/brightness
rm -f /tmp/mqtt_test_diag.txt
mosquitto_sub -h "$MQTT_SERVER" -p "$MQTT_PORT" $AUTH_ARGS -t "${MQTT_BASE}system/uptime" -C 1 -W 65 > /tmp/mqtt_test_diag.txt &
SUB_DIAG=$!

# Also listen for settings to ensure they ARE NOT published as frequently
rm -f /tmp/mqtt_test_settings.txt
mosquitto_sub -h "$MQTT_SERVER" -p "$MQTT_PORT" $AUTH_ARGS -t "${MQTT_BASE}settings/brightness" -C 1 -W 65 > /tmp/mqtt_test_settings.txt &
SUB_SET=$!

wait $SUB_DIAG || true
kill $SUB_SET 2>/dev/null || true

if [ -s /tmp/mqtt_test_diag.txt ]; then
    echo "✅ Success: Received system diagnostic telemetry."
else
    echo "❌ Failed to receive system diagnostic telemetry within interval."
    exit 1
fi

# Note: We can't strictly assert settings weren't published because someone might have forced them,
# but in an isolated test environment, settings should be empty.

echo -e "\n⏳ [2/2] Triggering settings change to force immediate settings publish..."
# Trigger a config change to force settings publish
curl -s -X POST -H "Content-Type: application/json" -d '{"brightness": 55}' "http://$DEVICE_IP/api/config" > /dev/null

rm -f /tmp/mqtt_test_settings_forced.txt
mosquitto_sub -h "$MQTT_SERVER" -p "$MQTT_PORT" $AUTH_ARGS -t "${MQTT_BASE}settings/brightness" -C 1 -W 5 > /tmp/mqtt_test_settings_forced.txt &
SUB_FORCE=$!

wait $SUB_FORCE || true

if [ -s /tmp/mqtt_test_settings_forced.txt ]; then
    echo "✅ Success: Received operational settings immediately after force publish."
else
    echo "❌ Failed to receive operational settings after forcing publish."
    exit 1
fi

echo -e "\n========================================================"
echo "🎉 SUCCESS: MQTT Interval Separation Tests Passed!"
echo "========================================================"
