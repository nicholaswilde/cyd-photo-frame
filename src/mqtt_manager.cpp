#include "mqtt_manager.h"

MqttManager::MqttManager(const String& server, uint16_t port, const String& user, const String& password, const String& baseTopic)
    : _server(server), _port(port), _user(user), _password(password), _baseTopic(baseTopic), _reconnectTimer(nullptr), _reconnectBackoffMs(5000), _messageCallback(nullptr) {}

void MqttManager::updateConfig(const String& server, uint16_t port, const String& user, const String& password, const String& baseTopic) {
    _server = server;
    _port = port;
    _user = user;
    _password = password;
    _baseTopic = baseTopic;
    _willTopic = _baseTopic + "status";
    
    // If we're already connected, we should disconnect and let it reconnect with new settings, 
    // or just apply credentials for the next reconnect.
    _mqttClient.setServer(_server.c_str(), _port);
    _mqttClient.setCredentials(_user.c_str(), _password.c_str());
    _mqttClient.setWill(_willTopic.c_str(), 1, true, "offline");
}

void MqttManager::begin() {
    // 1. Create a FreeRTOS timer for non-blocking reconnects.
    _reconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(5000), pdFALSE, (void*)this, onMqttReconnectTimer);

    // 2. Configure broker details
    _willTopic = _baseTopic + "status";
    _mqttClient.setServer(_server.c_str(), _port);
    _mqttClient.setCredentials(_user.c_str(), _password.c_str());
    _mqttClient.setWill(_willTopic.c_str(), 1, true, "offline");

    // 2.5 Configure unique Client ID
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    _clientId = "CYD-PhotoFrame-" + mac;
    _mqttClient.setClientId(_clientId.c_str());

    // 3. Register the asynchronous callbacks using C++ lambdas
    _mqttClient.onConnect([this](bool sessionPresent) {
        this->onMqttConnect(sessionPresent);
    });
    
    _mqttClient.onDisconnect([this](AsyncMqttClientDisconnectReason reason) {
        this->onMqttDisconnect(reason);
    });

    _mqttClient.onPublish([](uint16_t packetId) {
        Serial.printf("[MQTT] Broker acknowledged publish (Packet ID: %d)\n", packetId);
    });

    _mqttClient.onMessage([this](char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
        this->onMqttMessage(topic, payload, properties, len, index, total);
    });
}

void MqttManager::connectToMqtt() {
    Serial.printf("[MQTT] Connecting to broker at %s:%d...\n", _server.c_str(), _port);
    _mqttClient.connect();
}

void MqttManager::onNetworkAvailable() {
    Serial.println("[MQTT] Network is up. Initiating broker connection...");
    connectToMqtt();
}

void MqttManager::onNetworkDisconnected() {
    Serial.println("[MQTT] Network is down. Halting reconnect timers...");
    if (_reconnectTimer) {
        xTimerStop(_reconnectTimer, 0);
    }
}


static void publishTask(void* pvParameters) {
    MqttManager* mqtt = static_cast<MqttManager*>(pvParameters);
    mqtt->publishHADiscovery();
    vTaskDelete(NULL);
}

void MqttManager::onMqttConnect(bool sessionPresent) {
    Serial.println("[MQTT] Connected to broker!");
    
    // Reset backoff on successful connection
    _reconnectBackoffMs = 5000;
    
    // Publish a boot message
    _mqttClient.publish((_baseTopic + "status").c_str(), 0, true, "online");
    
    // Publish HA Discovery configuration
    xTaskCreate(publishTask, "pubTask", 4096, this, 1, NULL);

    // Subscribe to commands
    subscribe("command/brightness", 0);
    subscribe("command/reboot", 0);
    subscribe("command/auto_brightness", 0);
    subscribe("command/screensaver", 0);
    subscribe("command/screen_orientation", 0);
    subscribe("command/slideshow_interval", 0);
    subscribe("command/random_mode", 0);
    subscribe("command/show_filename", 0);
    subscribe("command/inactivity_sleep", 0);
    subscribe("command/bypass_optimization", 0);
    subscribe("command/boot_from_cache", 0);
    subscribe("command/api_server_enabled", 0);
    subscribe("command/theme", 0);
    subscribe("command/next_image", 0);
    subscribe("command/prev_image", 0);
    subscribe("command/toggle_play", 0);
    subscribe("command/led_light", 0);
    subscribe("command/led_brightness", 0);
    subscribe("command/clear_cache", 0);
}

void MqttManager::publishHADiscovery() {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String deviceId = "cyd_photoframe_" + mac;
    String deviceJson = "\"device\":{\"identifiers\":[\"" + deviceId + "\"],\"name\":\"CYD Photo Frame " + mac.substring(mac.length() - 4) + "\",\"manufacturer\":\"Nicholas Wilde\",\"model\":\"CYD-28R/35C\"}";

    // Connection Status
    String connPayload = "{\"name\":\"Connection Status\",\"state_topic\":\"" + _baseTopic + "status\",\"payload_on\":\"online\",\"payload_off\":\"offline\",\"device_class\":\"connectivity\",\"unique_id\":\"" + deviceId + "_conn\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/binary_sensor/" + deviceId + "/connection/config").c_str(), 0, true, connPayload.c_str());
    vTaskDelay(pdMS_TO_TICKS(50));

    // Brightness Control (Number)
    String brightPayload = "{\"name\":\"Brightness\",\"state_topic\":\"" + _baseTopic + "settings/brightness\",\"command_topic\":\"" + _baseTopic + "command/brightness\",\"min\":0,\"max\":255,\"entity_category\":\"config\",\"unique_id\":\"" + deviceId + "_bright\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/number/" + deviceId + "/brightness/config").c_str(), 0, true, brightPayload.c_str());
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Reboot Control (Button)
    String rebootPayload = "{\"name\":\"Reboot\",\"command_topic\":\"" + _baseTopic + "command/reboot\",\"payload_press\":\"REBOOT\",\"device_class\":\"restart\",\"unique_id\":\"" + deviceId + "_reboot\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/button/" + deviceId + "/reboot/config").c_str(), 0, true, rebootPayload.c_str());
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Next Image (Button)
    String nextPayload = "{\"name\":\"Next Image\",\"command_topic\":\"" + _baseTopic + "command/next_image\",\"payload_press\":\"NEXT\",\"unique_id\":\"" + deviceId + "_next\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/button/" + deviceId + "/next_image/config").c_str(), 0, true, nextPayload.c_str());
    
    // Prev Image (Button)
    String prevPayload = "{\"name\":\"Previous Image\",\"command_topic\":\"" + _baseTopic + "command/prev_image\",\"payload_press\":\"PREV\",\"unique_id\":\"" + deviceId + "_prev\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/button/" + deviceId + "/prev_image/config").c_str(), 0, true, prevPayload.c_str());
    
    // Toggle Play/Pause (Button)
    String togglePayload = "{\"name\":\"Play/Pause\",\"command_topic\":\"" + _baseTopic + "command/toggle_play\",\"payload_press\":\"TOGGLE\",\"unique_id\":\"" + deviceId + "_toggle\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/button/" + deviceId + "/toggle_play/config").c_str(), 0, true, togglePayload.c_str());
    vTaskDelay(pdMS_TO_TICKS(50));

    // --- System Diagnostics ---
    // Uptime
    String uptimePayload = "{\"name\":\"Uptime\",\"state_topic\":\"" + _baseTopic + "system/uptime\",\"unit_of_measurement\":\"s\",\"device_class\":\"duration\",\"entity_category\":\"diagnostic\",\"unique_id\":\"" + deviceId + "_uptime\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/sensor/" + deviceId + "/uptime/config").c_str(), 0, true, uptimePayload.c_str());
    // Free Heap
    String heapPayload = "{\"name\":\"Free Memory\",\"state_topic\":\"" + _baseTopic + "system/free_heap\",\"unit_of_measurement\":\"B\",\"device_class\":\"data_size\",\"entity_category\":\"diagnostic\",\"unique_id\":\"" + deviceId + "_heap\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/sensor/" + deviceId + "/free_heap/config").c_str(), 0, true, heapPayload.c_str());
    vTaskDelay(pdMS_TO_TICKS(50));
    // Wi-Fi RSSI
    String rssiPayload = "{\"name\":\"Wi-Fi Signal\",\"state_topic\":\"" + _baseTopic + "system/wifi_rssi\",\"unit_of_measurement\":\"dBm\",\"device_class\":\"signal_strength\",\"entity_category\":\"diagnostic\",\"unique_id\":\"" + deviceId + "_rssi\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/sensor/" + deviceId + "/wifi_rssi/config").c_str(), 0, true, rssiPayload.c_str());
    // IP Address
    String ipPayload = "{\"name\":\"IP Address\",\"state_topic\":\"" + _baseTopic + "system/ip\",\"entity_category\":\"diagnostic\",\"unique_id\":\"" + deviceId + "_ip\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/sensor/" + deviceId + "/ip/config").c_str(), 0, true, ipPayload.c_str());
    vTaskDelay(pdMS_TO_TICKS(50));
    // Firmware Version
    String verPayload = "{\"name\":\"Firmware Version\",\"state_topic\":\"" + _baseTopic + "system/version\",\"entity_category\":\"diagnostic\",\"unique_id\":\"" + deviceId + "_version\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/sensor/" + deviceId + "/version/config").c_str(), 0, true, verPayload.c_str());
    // MAC Address
    String macPayload = "{\"name\":\"MAC Address\",\"state_topic\":\"" + _baseTopic + "system/mac\",\"entity_category\":\"diagnostic\",\"unique_id\":\"" + deviceId + "_mac\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/sensor/" + deviceId + "/mac/config").c_str(), 0, true, macPayload.c_str());
    // Total Images
    String totalImgPayload = "{\"name\":\"Total Images\",\"state_topic\":\"" + _baseTopic + "state/image\",\"value_template\":\"{{ value_json.total_images }}\",\"icon\":\"mdi:image-multiple\",\"unique_id\":\"" + deviceId + "_total_images\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/sensor/" + deviceId + "/total_images/config").c_str(), 0, true, totalImgPayload.c_str());
    vTaskDelay(pdMS_TO_TICKS(50));

    // --- Operational Settings ---
    // Auto Brightness (Switch)
    String autoBrPayload = "{\"name\":\"Auto Brightness\",\"state_topic\":\"" + _baseTopic + "settings/auto_brightness\",\"command_topic\":\"" + _baseTopic + "command/auto_brightness\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"entity_category\":\"config\",\"unique_id\":\"" + deviceId + "_autobright\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/switch/" + deviceId + "/auto_brightness/config").c_str(), 0, true, autoBrPayload.c_str());
    // Random Mode (Switch)
    String randPayload = "{\"name\":\"Random Mode\",\"state_topic\":\"" + _baseTopic + "settings/random_mode\",\"command_topic\":\"" + _baseTopic + "command/random_mode\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"entity_category\":\"config\",\"unique_id\":\"" + deviceId + "_random\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/switch/" + deviceId + "/random_mode/config").c_str(), 0, true, randPayload.c_str());
    // Show Filename (Switch)
    String fnPayload = "{\"name\":\"Show Filename\",\"state_topic\":\"" + _baseTopic + "settings/show_filename\",\"command_topic\":\"" + _baseTopic + "command/show_filename\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"entity_category\":\"config\",\"unique_id\":\"" + deviceId + "_showfn\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/switch/" + deviceId + "/show_filename/config").c_str(), 0, true, fnPayload.c_str());
    vTaskDelay(pdMS_TO_TICKS(50));
    // Inactivity Sleep (Switch)
    String sleepPayload = "{\"name\":\"Inactivity Sleep\",\"state_topic\":\"" + _baseTopic + "settings/inactivity_sleep\",\"command_topic\":\"" + _baseTopic + "command/inactivity_sleep\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"entity_category\":\"config\",\"unique_id\":\"" + deviceId + "_insleep\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/switch/" + deviceId + "/inactivity_sleep/config").c_str(), 0, true, sleepPayload.c_str());
    vTaskDelay(pdMS_TO_TICKS(50));
    // Bypass Optimization (Switch)
    String bypassOptPayload = "{\"name\":\"Bypass Optimization\",\"state_topic\":\"" + _baseTopic + "settings/bypass_optimization\",\"command_topic\":\"" + _baseTopic + "command/bypass_optimization\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"entity_category\":\"config\",\"unique_id\":\"" + deviceId + "_bypassopt\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/switch/" + deviceId + "/bypass_optimization/config").c_str(), 0, true, bypassOptPayload.c_str());
    // Boot from Cache (Switch)
    String bootCachePayload = "{\"name\":\"Boot From Cache\",\"state_topic\":\"" + _baseTopic + "settings/boot_from_cache\",\"command_topic\":\"" + _baseTopic + "command/boot_from_cache\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:sd\",\"unique_id\":\"" + deviceId + "_boot_from_cache\",\"device\":{\"identifiers\":[\"" + deviceId + "\"]}}";
    _mqttClient.publish(("homeassistant/switch/" + deviceId + "/boot_from_cache/config").c_str(), 0, true, bootCachePayload.c_str());
    // API Server Enabled (Switch)
    String apiServerPayload = "{\"name\":\"API Server\",\"state_topic\":\"" + _baseTopic + "settings/api_server_enabled\",\"command_topic\":\"" + _baseTopic + "command/api_server_enabled\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:api\",\"unique_id\":\"" + deviceId + "_api_server_enabled\",\"device\":{\"identifiers\":[\"" + deviceId + "\"]}}";
    _mqttClient.publish(("homeassistant/switch/" + deviceId + "/api_server_enabled/config").c_str(), 0, true, apiServerPayload.c_str());

    // Theme (Select)
    String themePayload = "{\"name\":\"Theme Flavor\",\"state_topic\":\"" + _baseTopic + "settings/theme\",\"command_topic\":\"" + _baseTopic + "command/theme\",\"options\":[\"Mocha\",\"Macchiato\",\"Frappe\",\"Latte\"],\"entity_category\":\"config\",\"unique_id\":\"" + deviceId + "_theme\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/select/" + deviceId + "/theme/config").c_str(), 0, true, themePayload.c_str());
    // Screen Orientation (Select)
    String orientPayload = "{\"name\":\"Screen Orientation\",\"state_topic\":\"" + _baseTopic + "settings/screen_orientation\",\"command_topic\":\"" + _baseTopic + "command/screen_orientation\",\"options\":[\"Portrait Rev\",\"Landscape\",\"Portrait\",\"Landscape Rev\"],\"entity_category\":\"config\",\"unique_id\":\"" + deviceId + "_orientation\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/select/" + deviceId + "/orientation/config").c_str(), 0, true, orientPayload.c_str());
    vTaskDelay(pdMS_TO_TICKS(50));
    // Slideshow Interval (Select)
    String updPayload = "{\"name\":\"Slideshow Interval\",\"state_topic\":\"" + _baseTopic + "settings/slideshow_interval\",\"command_topic\":\"" + _baseTopic + "command/slideshow_interval\",\"options\":[\"2s\",\"5s\",\"10s\",\"15s\",\"30s\",\"60s\"],\"entity_category\":\"config\",\"unique_id\":\"" + deviceId + "_interval\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/select/" + deviceId + "/slideshow_interval/config").c_str(), 0, true, updPayload.c_str());
    // Clear out the old number entity if it still exists in HA's retained MQTT cache
    _mqttClient.publish(("homeassistant/number/" + deviceId + "/slideshow_interval/config").c_str(), 0, true, "");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Current Image (Sensor)
    String curImgPayload = "{\"name\":\"Current Image\",\"state_topic\":\"" + _baseTopic + "state/current_image\",\"icon\":\"mdi:image\",\"unique_id\":\"" + deviceId + "_cur_img\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/sensor/" + deviceId + "/current_image/config").c_str(), 0, true, curImgPayload.c_str());
    vTaskDelay(pdMS_TO_TICKS(50));

    // Playback State (Binary Sensor)
    String playStatePayload = "{\"name\":\"Playback State\",\"state_topic\":\"" + _baseTopic + "state/playback_state\",\"payload_on\":\"PLAYING\",\"payload_off\":\"PAUSED\",\"unique_id\":\"" + deviceId + "_play_state\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/binary_sensor/" + deviceId + "/playback_state/config").c_str(), 0, true, playStatePayload.c_str());
    vTaskDelay(pdMS_TO_TICKS(50));

    // Clear Cache (Button)
    String clrCachePayload = "{\"name\":\"Clear Cache\",\"command_topic\":\"" + _baseTopic + "command/clear_cache\",\"payload_press\":\"CLEAR\",\"icon\":\"mdi:delete-sweep\",\"unique_id\":\"" + deviceId + "_clear_cache\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/button/" + deviceId + "/clear_cache/config").c_str(), 0, true, clrCachePayload.c_str());
    vTaskDelay(pdMS_TO_TICKS(50));

    // LED Light (Switch)
    String ledLightPayload = "{\"name\":\"LED Light\",\"state_topic\":\"" + _baseTopic + "settings/led_light\",\"command_topic\":\"" + _baseTopic + "command/led_light\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:led-on\",\"unique_id\":\"" + deviceId + "_led_light\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/switch/" + deviceId + "/led_light/config").c_str(), 0, true, ledLightPayload.c_str());
    vTaskDelay(pdMS_TO_TICKS(50));

    // LED Brightness (Number)
    String ledBrightPayload = "{\"name\":\"LED Brightness\",\"state_topic\":\"" + _baseTopic + "settings/led_brightness\",\"command_topic\":\"" + _baseTopic + "command/led_brightness\",\"min\":0,\"max\":255,\"entity_category\":\"config\",\"icon\":\"mdi:brightness-6\",\"unique_id\":\"" + deviceId + "_led_bright\"," + deviceJson + "}";
    _mqttClient.publish(("homeassistant/number/" + deviceId + "/led_brightness/config").c_str(), 0, true, ledBrightPayload.c_str());
    vTaskDelay(pdMS_TO_TICKS(50));
}

void MqttManager::onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    Serial.printf("[MQTT] Disconnected from broker! Reason code: %d\n", (int8_t)reason);
    
    if ((int8_t)reason == 4) {
        Serial.println("[MQTT] Hint: Reason 4 usually means Bad Username or Password.");
    }

    Serial.printf("[MQTT] Reconnecting in %lu seconds...\n", _reconnectBackoffMs / 1000);
    
    // Only start the reconnect timer if Wi-Fi is still connected
    if (WiFi.status() == WL_CONNECTED && _reconnectTimer) {
        xTimerChangePeriod(_reconnectTimer, pdMS_TO_TICKS(_reconnectBackoffMs), 0);
        xTimerStart(_reconnectTimer, 0);
        
        // Increase backoff for next time, capped at max limit (e.g., 2 minutes / 120000ms)
        _reconnectBackoffMs *= 2;
        if (_reconnectBackoffMs > 120000) {
            _reconnectBackoffMs = 120000;
        }
    }
}

void MqttManager::onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    if (_messageCallback) {
        String payloadStr;
        for (size_t i = 0; i < len; i++) {
            payloadStr += (char)payload[i];
        }
        Serial.printf("[MQTT] Received message on topic '%s': %s\n", topic, payloadStr.c_str());
        _messageCallback(String(topic), payloadStr);
    }
}

bool MqttManager::isConnected() {
    return _mqttClient.connected();
}

void MqttManager::publish(const char* topic, const char* payload, bool retain) {
    if (isConnected()) {
        String fullTopic = String(topic);
        if (!fullTopic.startsWith("homeassistant/")) {
            fullTopic = _baseTopic + fullTopic;
        }
        Serial.printf("[MQTT] Publishing -> Topic: '%s' | Retain: %d | Payload: '%s'\n", fullTopic.c_str(), retain, payload);
        uint16_t packetId = _mqttClient.publish(fullTopic.c_str(), 0, retain, payload);
        
        if (packetId == 0) {
            Serial.println("[MQTT] ERROR: Publish failed (buffer might be full)");
        }
    } else {
        Serial.printf("[MQTT] WARN: Cannot publish to '%s' - Not connected to broker.\n", topic);
    }
}

void MqttManager::subscribe(const char* topic, uint8_t qos) {
    if (isConnected()) {
        String fullTopic = String(topic);
        if (!fullTopic.startsWith("homeassistant/")) {
            fullTopic = _baseTopic + fullTopic;
        }
        _mqttClient.subscribe(fullTopic.c_str(), qos);
        Serial.printf("[MQTT] Subscribed to topic: %s\n", fullTopic.c_str());
    } else {
        Serial.printf("[MQTT] WARN: Cannot subscribe to '%s' - Not connected to broker.\n", topic);
    }
}

void MqttManager::onMessage(MqttMessageCallback cb) {
    _messageCallback = cb;
}

void MqttManager::disconnect() {
    Serial.println("[MQTT] Disconnecting from broker...");
    if (_reconnectTimer) {
        xTimerStop(_reconnectTimer, 0);
    }
    _mqttClient.disconnect();
}


void MqttManager::onMqttReconnectTimer(TimerHandle_t xTimer) {
    // Retrieve the class instance pointer from the timer ID
    MqttManager* instance = static_cast<MqttManager*>(pvTimerGetTimerID(xTimer));
    if (instance) {
        instance->connectToMqtt();
    }
}
