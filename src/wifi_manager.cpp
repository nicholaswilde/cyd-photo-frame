#include "wifi_manager.h"

#ifndef NATIVE_TEST
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SD.h>
#include "config/config.h"
#include "screenshot_manager.h"
#include "lvgl_manager.h"

WifiManager::WifiManager(const std::string& ssid, const std::string& password)
    : _ssid(ssid), _password(password), _state(WIFI_STATE_DISCONNECTED), _lastReconnectAttempt(0), _connectionStartTime(0) {}

void WifiManager::begin() {
    Serial.println("[WiFi] Starting Wi-Fi Manager...");
    WiFi.setAutoReconnect(true);
    WiFi.setTxPower(WIFI_POWER_11dBm);
    WiFi.mode(WIFI_STA);
    
    if (_ssid.empty()) {
        Serial.println("[WiFi] No credentials configured. Launching AP mode directly...");
        startAPMode();
    } else {
        WiFi.begin(_ssid.c_str(), _password.c_str());
        _state = WIFI_STATE_CONNECTING;
        _connectionStartTime = millis();
        Serial.printf("[WiFi] Connecting to SSID: %s...\n", _ssid.c_str());
    }
}

void WifiManager::stop() {
    _state = WIFI_STATE_DISCONNECTED;
    if (_dnsServer) {
        ((DNSServer*)_dnsServer)->stop();
    }
    if (_webServer) {
        ((WebServer*)_webServer)->stop();
    }
    stopScreenshotServer();
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("[WiFi] Wi-Fi stopped.");
}

void WifiManager::update() {
    wl_status_t status = WiFi.status();

    switch (_state) {
        case WIFI_STATE_DISCONNECTED:
            if (millis() - _lastReconnectAttempt > _reconnectInterval) {
                _lastReconnectAttempt = millis();
                Serial.println("[WiFi] Reconnecting...");
                WiFi.begin(_ssid.c_str(), _password.c_str());
                _state = WIFI_STATE_CONNECTING;
                _connectionStartTime = millis();
            }
            break;

        case WIFI_STATE_CONNECTING:
            if (status == WL_CONNECTED) {
                _state = WIFI_STATE_CONNECTED;
                Serial.print("[WiFi] Connected! IP address: ");
                Serial.println(WiFi.localIP());
                startScreenshotServer();
            } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL || (millis() - _connectionStartTime > _connectionTimeout)) {
                Serial.println("[WiFi] Connection failed or timed out. Transitioning to AP Mode...");
                startAPMode();
            }
            break;

        case WIFI_STATE_CONNECTED:
            if (status != WL_CONNECTED) {
                _state = WIFI_STATE_DISCONNECTED;
                _lastReconnectAttempt = millis();
                Serial.println("[WiFi] Connection lost.");
                stopScreenshotServer();
            } else if (_webServer) {
                ((WebServer*)_webServer)->handleClient();
            }
            break;

        case WIFI_STATE_AP_MODE:
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[WiFi] Wi-Fi connected in background. Stopping AP Mode...");
                if (_dnsServer) {
                    ((DNSServer*)_dnsServer)->stop();
                    delete (DNSServer*)_dnsServer;
                    _dnsServer = nullptr;
                }
                if (_webServer) {
                    ((WebServer*)_webServer)->stop();
                    delete (WebServer*)_webServer;
                    _webServer = nullptr;
                }
                WiFi.softAPdisconnect(true);
                WiFi.mode(WIFI_STA);
                _state = WIFI_STATE_CONNECTED;
                Serial.print("[WiFi] Connected! IP address: ");
                Serial.println(WiFi.localIP());
                startScreenshotServer();
            } else {
                if (_dnsServer) ((DNSServer*)_dnsServer)->processNextRequest();
                if (_webServer) ((WebServer*)_webServer)->handleClient();
            }
            break;
    }
}

WifiState WifiManager::getState() const {
    return _state;
}

std::string WifiManager::getIPAddress() const {
    if (_state == WIFI_STATE_CONNECTED) {
        return WiFi.localIP().toString().c_str();
    } else if (_state == WIFI_STATE_AP_MODE) {
        return "192.168.4.1";
    }
    return "0.0.0.0";
}

std::string WifiManager::getAPSSID() const {
    String mac = WiFi.macAddress();
    std::string cleanMac = "";
    for (size_t i = 0; i < mac.length(); i++) {
        if (mac[i] != ':') {
            cleanMac += mac[i];
        }
    }
    std::string suffix = "";
    if (cleanMac.length() >= 4) {
        suffix = cleanMac.substr(cleanMac.length() - 4);
    } else {
        suffix = "ESP32";
    }
    for (size_t i = 0; i < suffix.length(); i++) {
        suffix[i] = toupper(suffix[i]);
    }
    return "cyd-photo-frame-" + suffix;
}

void WifiManager::startAPMode() {
    _state = WIFI_STATE_AP_MODE;
    std::string apSSID = getAPSSID();
    Serial.printf("[WiFi] Entering AP Mode. SSID: %s\n", apSSID.c_str());

    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect();
    delay(200);

    WiFi.mode(WIFI_AP_STA);
    WiFi.setTxPower(WIFI_POWER_11dBm);
    delay(100);

    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    delay(100);
    
    // No password by default for AP mode
    WiFi.softAP(apSSID.c_str(), nullptr);
    delay(200);

    _cachedNetworksHTML = "<div class='net-item' style='color: #a6adc8;'>Scanning in progress... Please refresh.</div>";
    WiFi.scanNetworks(true, false, false, 150);

    DNSServer* dns = new DNSServer();
    dns->setErrorReplyCode(DNSReplyCode::NoError);
    dns->start(53, "*", apIP);
    _dnsServer = dns;

    WebServer* server = new WebServer(80);
    server->on("/", [this]() { handleRoot(); });
    server->on("/save", [this]() { handleSave(); });
    server->on("/scan", [this]() {
        WiFi.scanNetworks(true, false, false, 150);
        WebServer* s = (WebServer*)_webServer;
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta http-equiv='refresh' content='3;url=/'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<title>Scanning...</title>";
        html += "<style>";
        html += "body { font-family: 'Inter', system-ui, sans-serif; background: #1e1e2e; color: #cdd6f4; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; }";
        html += ".card { background: #181825; border-radius: 12px; padding: 30px; width: 100%; max-width: 400px; box-shadow: 0 8px 30px rgba(0,0,0,0.3); border: 1px solid #313244; text-align: center; }";
        html += "h2 { color: #f5c2e7; margin-top: 0; }";
        html += "p { color: #a6adc8; }";
        html += "</style></head><body>";
        html += "<div class='card'><h2>Scanning for Wi-Fi...</h2><p>Please wait while we refresh the network list.</p></div>";
        html += "</body></html>";
        s->send(200, "text/html", html);
    });
    server->onNotFound([this]() { handleNotFound(); });
    server->begin();
    _webServer = server;

    Serial.println("[WiFi] AP Mode Web Server and DNS Server started.");
}

void WifiManager::handleRoot() {
    WebServer* server = (WebServer*)_webServer;
    int16_t scanStatus = WiFi.scanComplete();
    if (scanStatus >= 0) {
        _cachedNetworksHTML = "";
        for (int i = 0; i < scanStatus; ++i) {
            String ssidName = WiFi.SSID(i);
            int32_t rssi = WiFi.RSSI(i);
            std::string ssidStr = ssidName.c_str();
            _cachedNetworksHTML += "<div class='net-item' onclick='selectSSID(\"" + ssidStr + "\")'>";
            _cachedNetworksHTML += "<span>" + ssidStr + "</span>";
            _cachedNetworksHTML += "<span style='color: #a6adc8; font-size: 12px;'>" + std::to_string(rssi) + " dBm</span>";
            _cachedNetworksHTML += "</div>";
        }
        WiFi.scanDelete();
    } else if (scanStatus == WIFI_SCAN_FAILED) {
        if (_cachedNetworksHTML.length() == 0 || _cachedNetworksHTML.find("Scanning in progress") != std::string::npos) {
            _cachedNetworksHTML = "<div class='net-item' style='color: #a6adc8;'>No networks found</div>";
        }
    }

    String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>CYD Photo Frame WiFi Setup</title>";
    html += "<style>";
    html += "body { font-family: 'Inter', system-ui, sans-serif; background: #1e1e2e; color: #cdd6f4; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; box-sizing: border-box; }";
    html += ".card { background: #181825; border-radius: 12px; padding: 30px; width: 100%; max-width: 400px; box-shadow: 0 8px 30px rgba(0,0,0,0.3); border: 1px solid #313244; }";
    html += "h2 { color: #f5c2e7; margin-top: 0; margin-bottom: 20px; font-weight: 600; text-align: center; }";
    html += "label { display: block; margin-bottom: 8px; color: #a6adc8; font-size: 14px; }";
    html += "select, input[type='text'], input[type='password'] { width: 100%; padding: 12px; margin-bottom: 20px; border-radius: 6px; border: 1px solid #45475a; background: #313244; color: #cdd6f4; font-size: 16px; box-sizing: border-box; }";
    html += "select:focus, input:focus { outline: none; border-color: #f5c2e7; }";
    html += "button { width: 100%; padding: 12px; background: #cba6f7; border: none; border-radius: 6px; color: #11111b; font-size: 16px; font-weight: bold; cursor: pointer; transition: background 0.2s; }";
    html += "button:hover { background: #f5c2e7; }";
    html += ".net-list { margin-bottom: 20px; max-height: 150px; overflow-y: auto; border: 1px solid #313244; border-radius: 6px; padding: 10px; background: #11111b; }";
    html += ".net-item { display: flex; justify-content: space-between; padding: 8px; cursor: pointer; border-bottom: 1px solid #1e1e2e; }";
    html += ".net-item:last-child { border-bottom: none; }";
    html += ".net-item:hover { background: #313244; color: #f5c2e7; }";
    html += "</style>";
    html += "<script>";
    html += "function selectSSID(ssid) { document.getElementById('ssid').value = ssid; }";
    html += "</script>";
    html += "</head><body>";
    html += "<div class='card'>";
    html += "<h2>Wi-Fi Configuration</h2>";
    html += "<form method='POST' action='/save'>";
    
    html += "<div style='display: flex; justify-content: space-between; align-items: center;'>";
    html += "<label style='margin-bottom: 0;'>Select Network</label>";
    html += "<a href='/scan' style='color: #cba6f7; font-size: 12px; text-decoration: none;'>🔄 Refresh List</a>";
    html += "</div>";
    html += "<div style='height: 8px;'></div>";
    
    html += "<div class='net-list'>";
    html += _cachedNetworksHTML.c_str();
    html += "</div>";
    
    html += "<label for='ssid'>SSID</label>";
    html += "<input type='text' id='ssid' name='ssid' placeholder='SSID name' required>";
    
    html += "<label for='pass'>Password</label>";
    html += "<input type='password' id='pass' name='pass' placeholder='Password'>";
    
    html += "<button type='submit'>Save & Connect</button>";
    html += "</form>";
    html += "</div>";
    html += "</body></html>";

    server->send(200, "text/html", html);
}

void WifiManager::handleSave() {
    WebServer* server = (WebServer*)_webServer;
    String ssid = server->arg("ssid");
    String pass = server->arg("pass");

    Serial.printf("[WiFi] Saved new credentials via captive portal: %s\n", ssid.c_str());

    String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>Credentials Saved</title>";
    html += "<style>";
    html += "body { font-family: 'Inter', system-ui, sans-serif; background: #1e1e2e; color: #cdd6f4; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; box-sizing: border-box; }";
    html += ".card { background: #181825; border-radius: 12px; padding: 30px; width: 100%; max-width: 400px; box-shadow: 0 8px 30px rgba(0,0,0,0.3); border: 1px solid #313244; text-align: center; }";
    html += "h2 { color: #a6e3a1; margin-top: 0; margin-bottom: 20px; }";
    html += "p { color: #cdd6f4; margin-bottom: 20px; line-height: 1.5; }";
    html += "</style></head><body>";
    html += "<div class='card'>";
    html += "<h2>Configuration Saved</h2>";
    html += "<p>Connecting to <strong>" + ssid + "</strong>...</p>";
    html += "<p>The device will now reboot to apply the new settings. You can close this page.</p>";
    html += "</div>";
    html += "</body></html>";

    server->send(200, "text/html", html);
    delay(2000);

    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", pass);
    prefs.putBool("wifi_on", true); // Keep WiFi enabled
    prefs.end();

    ESP.restart();
}

void WifiManager::handleNotFound() {
    WebServer* server = (WebServer*)_webServer;
    server->sendHeader("Location", "http://192.168.4.1/", true);
    server->send(302, "text/plain", "");
}

void WifiManager::startScreenshotServer() {
    if (_webServer) {
        stopScreenshotServer();
    }
    WebServer* server = new WebServer(80);
    server->on("/screenshot", [this]() { handleScreenshot(); });
    server->on("/api/orientation", HTTP_POST, [this]() { handleOrientation(); });
    server->on("/api/screen", HTTP_POST, [this]() { handleScreen(); });
    server->begin();
    _webServer = server;
    Serial.println("[WiFi] Screenshot server started on port 80.");
}

void WifiManager::stopScreenshotServer() {
    if (_webServer) {
        ((WebServer*)_webServer)->stop();
        delete (WebServer*)_webServer;
        _webServer = nullptr;
        Serial.println("[WiFi] Screenshot server stopped.");
    }
}

void WifiManager::handleScreenshot() {
    WebServer* server = (WebServer*)_webServer;
    const char* tmpPath = "/~scr_tmp.bmp";

    if (!ScreenshotManager::captureToSD(tmpPath)) {
        server->send(500, "text/plain", "Error: Screenshot capture failed");
        return;
    }

    File f = SD.open(tmpPath, FILE_READ);
    if (!f) {
        server->send(500, "text/plain", "Error: Cannot open temp screenshot file");
        SD.remove(tmpPath);
        return;
    }

    const uint32_t totalSize = f.size();
    server->setContentLength(totalSize);
    server->send(200, "image/bmp", "");

    WiFiClient client = server->client();
    uint8_t xferBuf[512];
    while (f.available()) {
        size_t n = f.read(xferBuf, sizeof(xferBuf));
        if (n > 0) {
            client.write(xferBuf, n);
        }
    }

    f.close();
    SD.remove(tmpPath);
    Serial.println("[WiFi] Screenshot streamed to remote client.");
}

void WifiManager::handleOrientation() {
    WebServer* server = (WebServer*)_webServer;
    if (server->hasArg("val")) {
        int rot = server->arg("val").toInt();
        Preferences prefs;
        prefs.begin("settings", false);
        prefs.putInt("orientation", rot);
        prefs.putInt("cached_rot", rot);
        prefs.end();
        server->send(200, "text/plain", "Orientation set. Rebooting...");
        delay(1000);
        ESP.restart();
    } else {
        server->send(400, "text/plain", "Missing 'val' argument");
    }
}

void WifiManager::handleScreen() {
    WebServer* server = (WebServer*)_webServer;
    if (server->hasArg("index")) {
        String tab = server->arg("index");
        if (tab == "settings") LVGLManager::showSettings();
        else if (tab == "sd_error") LVGLManager::showSDError();
        else if (tab == "warn") LVGLManager::showNoPhotosWarning();
        else if (tab == "opt") LVGLManager::showOptimizationScreen();
        else if (tab == "ap") LVGLManager::showAPModeScreen("TEST_AP", "192.168.4.1");
        else if (tab == "clear_cache") LVGLManager::showClearCacheScreen();
        server->send(200, "text/plain", "Screen switched.");
    } else {
        server->send(400, "text/plain", "Missing 'index' argument");
    }
}

#else
// Mock implementation for host-native tests
#include <string>

WifiManager::WifiManager(const std::string& ssid, const std::string& password)
    : _ssid(ssid), _password(password), _state(WIFI_STATE_DISCONNECTED), _lastReconnectAttempt(0), _connectionStartTime(0) {}

void WifiManager::begin() {
    _state = WIFI_STATE_CONNECTED;
}

void WifiManager::stop() {
    _state = WIFI_STATE_DISCONNECTED;
}

void WifiManager::update() {}

WifiState WifiManager::getState() const {
    return _state;
}

std::string WifiManager::getIPAddress() const {
    return "127.0.0.1";
}

std::string WifiManager::getAPSSID() const {
    return "cyd-photo-frame-mock";
}

void WifiManager::startAPMode() {}
void WifiManager::handleRoot() {}
void WifiManager::handleSave() {}
void WifiManager::handleNotFound() {}
void WifiManager::startScreenshotServer() {}
void WifiManager::stopScreenshotServer() {}
void WifiManager::handleScreenshot() {}
void WifiManager::handleOrientation() {}
void WifiManager::handleScreen() {}
#endif
