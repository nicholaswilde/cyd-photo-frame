#include "wifi_manager.h"

#ifndef NATIVE_TEST
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <ImprovWiFiLibrary.h>
#include <Preferences.h>
#include <SD.h>
#include "config/config.h"
#include "version.h"
#include "screenshot_manager.h"
#include "lvgl_manager.h"
#include "dashboard_html.h"
#include "settings_html.h"
#include "hardware_logic.h"

static void configureStaticIP() {
#ifndef NATIVE_TEST
#ifdef STATIC_IP
    IPAddress local_ip;
    if (local_ip.fromString(STATIC_IP)) {
        IPAddress gateway_ip;
        IPAddress subnet_ip;
        IPAddress dns_ip;

        #ifdef STATIC_GATEWAY
        gateway_ip.fromString(STATIC_GATEWAY);
        #endif
        #ifdef STATIC_SUBNET
        subnet_ip.fromString(STATIC_SUBNET);
        #endif
        #ifdef STATIC_DNS
        dns_ip.fromString(STATIC_DNS);
        #endif

        if (WiFi.config(local_ip, gateway_ip, subnet_ip, dns_ip)) {
            Serial.println("[WiFi] Static IP configured successfully.");
        } else {
            Serial.println("[WiFi] Failed to configure Static IP.");
        }
    }
#endif
#endif
}

WifiManager::WifiManager(const std::string& ssid, const std::string& password)
    : _ssid(ssid), _password(password), _state(WIFI_STATE_DISCONNECTED), _lastReconnectAttempt(0), _connectionStartTime(0) {}

void WifiManager::begin() {
    Serial.println("[WiFi] Starting Wi-Fi Manager...");
#ifndef NATIVE_TEST
    WiFi.setAutoReconnect(true);
    WiFi.setTxPower(WIFI_POWER_11dBm);

    ImprovWiFi* improv = new ImprovWiFi(&Serial);
    improv->setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32, "CYD-Photo-Frame", "1.0", "CYD Photo Frame", "http://{LOCAL_IPV4}");
    
    improv->setCustomConnectWiFi([](const char *ssid, const char *password) {
        Serial.printf("\n[WiFi] Improv connecting to %s...\n", ssid);
        // Turn off AP mode to speed up STA connection and avoid channel conflicts
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
        
        WiFi.begin(ssid, password);
        int attempts = 0;
        // Wait up to 8 seconds (16 * 500ms) to prevent browser RPC timeout (usually 10s)
        while (WiFi.status() != WL_CONNECTED && attempts < 16) { 
            delay(500);
            attempts++;
        }
        return WiFi.status() == WL_CONNECTED;
    });

    improv->onImprovConnected([](const char *ssid, const char *password) {
        Serial.printf("\n[WiFi] Improv provisioned: %s\n", ssid);
        Preferences prefs;
        prefs.begin("settings", false);
        prefs.putString("wifi_ssid", ssid);
        prefs.putString("wifi_pass", password);
        prefs.putBool("wifi_on", true);
        prefs.end();
        delay(500);
        ESP.restart();
    });
    _improv = improv;
#endif
    WiFi.mode(WIFI_STA);
    
    if (_ssid.empty()) {
        Serial.println("[WiFi] No credentials configured. Launching AP mode directly...");
        startAPMode();
    } else {
        configureStaticIP();
        WiFi.begin(_ssid.c_str(), _password.c_str());
        _state = WIFI_STATE_CONNECTING;
        _connectionStartTime = millis();
        Serial.printf("[WiFi] Connecting to SSID: %s...\n", _ssid.c_str());
    }
}

void WifiManager::stop() {
    _state = WIFI_STATE_STOPPED;
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
#ifndef NATIVE_TEST
    if (_improv) {
        ((ImprovWiFi*)_improv)->handleSerial();
    }
#endif
    wl_status_t status = WiFi.status();

    switch (_state) {
        case WIFI_STATE_DISCONNECTED:
            if (millis() - _lastReconnectAttempt > _reconnectInterval) {
                _lastReconnectAttempt = millis();
                Serial.println("[WiFi] Reconnecting...");
                configureStaticIP();
                WiFi.begin(_ssid.c_str(), _password.c_str());
                _state = WIFI_STATE_CONNECTING;
                _connectionStartTime = millis();
            }
            break;

        case WIFI_STATE_STOPPED:
            // Do nothing when stopped
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

static File _uploadFile;

const char uploadHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CYD Photo Frame Upload</title>
<style>
body { font-family: 'Inter', system-ui, sans-serif; background: #1e1e2e; color: #cdd6f4; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; box-sizing: border-box; flex-direction: column; }
.header { margin-bottom: 20px; text-align: center; }
.header h1 { color: #f5c2e7; margin: 0; font-weight: 700; font-size: 28px; }
.card { background: #181825; border-radius: 12px; padding: 30px; width: 100%; max-width: 500px; box-shadow: 0 8px 30px rgba(0,0,0,0.3); border: 1px solid #313244; }
h2 { color: #cdd6f4; margin-top: 0; margin-bottom: 20px; font-weight: 600; text-align: center; font-size: 20px; }
.drop-zone { border: 2px dashed #45475a; border-radius: 8px; padding: 40px 20px; cursor: pointer; transition: all 0.2s; background: #11111b; margin-bottom: 20px; text-align: center; }
.drop-zone.dragover { border-color: #cba6f7; background: #313244; }
.drop-zone p { color: #a6adc8; margin: 0; pointer-events: none; font-size: 16px; line-height: 1.5; }
.progress-list { margin-bottom: 15px; max-height: 250px; overflow-y: auto; }
.file-item { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; font-size: 14px; background: #313244; padding: 12px; border-radius: 6px; }
.file-info { display: flex; justify-content: space-between; width: 100%; align-items: center; }
.file-name { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; max-width: 60%; }
.file-status { color: #a6adc8; font-weight: bold; font-size: 13px; }
.remove-btn { background: #f38ba8; color: #11111b; border: none; border-radius: 4px; padding: 4px 10px; cursor: pointer; font-size: 12px; font-weight: bold; margin-left: 10px; transition: background 0.2s; }
.remove-btn:hover { background: #e78284; }
.remove-btn:disabled { background: #45475a; cursor: not-allowed; color: #a6adc8; }
.success { color: #a6e3a1; }
.error { color: #f38ba8; }
input[type="file"] { display: none; }
.upload-btn { width: 100%; padding: 12px; background: #cba6f7; border: none; border-radius: 6px; color: #11111b; font-size: 16px; font-weight: bold; cursor: pointer; transition: background 0.2s; display: none; }
.upload-btn:hover { background: #f5c2e7; }
.upload-btn:disabled { background: #45475a; cursor: not-allowed; color: #a6adc8; }
.restart-btn { width: 100%; padding: 12px; background: #f38ba8; border: none; border-radius: 6px; color: #11111b; font-size: 16px; font-weight: bold; cursor: pointer; transition: background 0.2s; display: none; margin-top: 10px; }
.restart-btn:hover { background: #e78284; }
.version { color: #a6adc8; font-size: 14px; margin-top: 5px; margin-bottom: 0; }
</style>
</head>
<body>
<div class="header">
    <h1>CYD Photo Frame</h1>
    <p class="version">)rawliteral" APP_VERSION R"rawliteral(</p>
</div>
<div class="card">
<h2>Upload Images</h2>
<div class="drop-zone" id="dropZone">
    <p>Drag & Drop images here<br>or click to browse</p>
</div>
<input type="file" id="fileInput" multiple accept="image/*,video/*">
<div class="progress-list" id="progressList"></div>
<button id="uploadBtn" class="upload-btn">Upload All</button>
<button id="restartBtn" class="restart-btn">Restart Device</button>
</div>
<script>
const dropZone = document.getElementById('dropZone');
const fileInput = document.getElementById('fileInput');
const progressList = document.getElementById('progressList');
const uploadBtn = document.getElementById('uploadBtn');
const restartBtn = document.getElementById('restartBtn');
let filesQueue = [];
let isUploading = false;

dropZone.addEventListener('click', () => { if (!isUploading) fileInput.click(); });
dropZone.addEventListener('dragover', (e) => { e.preventDefault(); if(!isUploading) dropZone.classList.add('dragover'); });
dropZone.addEventListener('dragleave', () => dropZone.classList.remove('dragover'));
dropZone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropZone.classList.remove('dragover');
    if(!isUploading) handleFiles(e.dataTransfer.files);
});
fileInput.addEventListener('change', (e) => handleFiles(e.target.files));
uploadBtn.addEventListener('click', () => {
    if (filesQueue.length > 0) {
        isUploading = true;
        uploadBtn.disabled = true;
        dropZone.style.opacity = '0.5';
        dropZone.style.cursor = 'not-allowed';
        document.querySelectorAll('.remove-btn').forEach(b => b.disabled = true);
        processQueue();
    }
});

function handleFiles(files) {
    if(isUploading) return;
    for (let f of files) {
        let id = 'file-' + Math.random().toString(36).substr(2, 9);
        let el = document.createElement('div');
        el.className = 'file-item';
        el.id = 'item-' + id;
        el.innerHTML = `
            <div class="file-info">
                <span class="file-name">${f.name}</span>
                <div>
                    <span class='file-status' id='${id}'>Queued</span>
                    <button class="remove-btn" onclick="removeFile('${id}')">X</button>
                </div>
            </div>`;
        progressList.appendChild(el);
        filesQueue.push({file: f, id: id});
    }
    updateUploadButton();
}

window.removeFile = function(id) {
    if(isUploading) return;
    filesQueue = filesQueue.filter(item => item.id !== id);
    document.getElementById('item-' + id).remove();
    updateUploadButton();
}

function updateUploadButton() {
    let pending = filesQueue.filter(i => document.getElementById(i.id).textContent === 'Queued').length;
    if (pending > 0 && !isUploading) {
        uploadBtn.style.display = 'block';
        uploadBtn.disabled = false;
        uploadBtn.textContent = `Upload All (${pending})`;
    } else if (!isUploading) {
        uploadBtn.style.display = 'none';
    }
}

async function processQueue() {
    let item = filesQueue.find(i => document.getElementById(i.id).textContent === 'Queued');
    if (!item) {
        isUploading = false;
        dropZone.style.opacity = '1';
        dropZone.style.cursor = 'pointer';
        uploadBtn.textContent = 'Upload Complete!';
        restartBtn.style.display = 'block';
        setTimeout(() => {
            updateUploadButton();
        }, 2000);
        return;
    }
    
    let statusEl = document.getElementById(item.id);
    let removeBtn = document.querySelector(`#item-${item.id} .remove-btn`);
    if(removeBtn) removeBtn.style.display = 'none';
    
    statusEl.textContent = 'Uploading...';
    
    let formData = new FormData();
    formData.append('f', item.file);
    
    try {
        let res = await fetch('/upload', { method: 'POST', body: formData });
        if (res.ok) {
            statusEl.textContent = 'Done';
            statusEl.className = 'file-status success';
        } else {
            statusEl.textContent = 'Error';
            statusEl.className = 'file-status error';
        }
    } catch(e) {
        statusEl.textContent = 'Failed';
        statusEl.className = 'file-status error';
    }
    processQueue();
}

restartBtn.addEventListener('click', async () => {
    restartBtn.textContent = 'Restarting...';
    restartBtn.disabled = true;
    try {
        await fetch('/restart', { method: 'POST' });
    } catch(e) {}
    setTimeout(() => {
        window.location.reload();
    }, 4000);
});
</script>
</body>
</html>
)rawliteral";

void WifiManager::startScreenshotServer() {
    if (_webServer) {
        stopScreenshotServer();
    }
    WebServer* server = new WebServer(80);
    server->on("/screenshot", [this]() { handleScreenshot(); });
    server->on("/api/orientation", HTTP_POST, [this]() { handleOrientation(); });
    server->on("/api/screen", HTTP_POST, [this]() { handleScreen(); });

    server->on("/", HTTP_GET, [this]() { handleDashboard(); });
    server->on("/settings", HTTP_GET, [this]() { handleSettings(); });
    server->on("/settings/save", HTTP_POST, [this]() { handleSettingsSave(); });

    server->on("/upload", HTTP_GET, [this]() {
        WebServer* s = (WebServer*)_webServer;
        s->send_P(200, "text/html", uploadHtml);
    });

    server->on("/restart", HTTP_POST, [this]() {
        WebServer* s = (WebServer*)_webServer;
        s->send(200, "text/plain", "Restarting...");
        delay(500);
        ESP.restart();
    });

    server->on("/upload", HTTP_POST, [this]() {
        WebServer* s = (WebServer*)_webServer;
        s->send(200, "text/plain", "OK");
    }, [this]() {
        WebServer* s = (WebServer*)_webServer;
        HTTPUpload& upload = s->upload();
        if (upload.status == UPLOAD_FILE_START) {
            String filename = upload.filename;
            if (!filename.startsWith("/")) filename = "/" + filename;
            Serial.printf("[Upload] Start: %s\n", filename.c_str());
            _uploadFile = SD.open(filename, FILE_WRITE);
            if (!_uploadFile) {
                Serial.printf("[Upload] ERROR: Could not open %s for writing!\n", filename.c_str());
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (_uploadFile) {
                size_t written = _uploadFile.write(upload.buf, upload.currentSize);
                if (written != upload.currentSize) {
                    Serial.printf("[Upload] ERROR: Wrote %u bytes, but expected %u\n", written, upload.currentSize);
                } else {
                    Serial.printf("[Upload] Wrote chunk of %u bytes\n", upload.currentSize);
                }
            } else {
                Serial.println("[Upload] ERROR: Cannot write chunk, file is not open");
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (_uploadFile) {
                _uploadFile.close();
                Serial.printf("[Upload] End: %s, Total Size: %u\n", upload.filename.c_str(), upload.totalSize);
            } else {
                Serial.println("[Upload] End: ERROR (File was never opened)");
            }
        } else if (upload.status == UPLOAD_FILE_ABORTED) {
            if (_uploadFile) {
                _uploadFile.close();
            }
            Serial.println("[Upload] ABORTED");
        }
    });

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
        else if (tab == "loading") LVGLManager::showLoadingSlideshowScreen("Loading slideshow...", false);
        else if (tab == "resuming") LVGLManager::showLoadingSlideshowScreen("Resuming slideshow...", false);
        server->send(200, "text/plain", "Screen switched.");
    } else {
        server->send(400, "text/plain", "Missing 'index' argument");
    }
}

void WifiManager::handleDashboard() {
    WebServer* server = (WebServer*)_webServer;
    String html = String(dashboard_html);
    html.replace("%APP_VERSION%", APP_VERSION);
    server->send(200, "text/html", html);
}

void WifiManager::handleSettings() {
    WebServer* server = (WebServer*)_webServer;
    Preferences prefs;
    prefs.begin("settings", true);
    
    int brightness = 255;
    bool autoBright = false;
    unsigned long delayMs = 10000;
    bool randomMode = false;
    bool showFilename = false;
    bool inactivitySleep = false;
    int themeFlavor = 0;
    int screenOrientation = 1;
    int ledBrightness = 128;
    bool isLedEnabled = true;
    bool isWifiEnabled = true;
    bool isMqttEnabled = false;
    std::string wifiSSID = "";
    std::string wifiPassword = "";
    bool bypassOptimization = false;
    bool bootFromCache = false;
    
    HardwareLogic::loadSettings(prefs, brightness, autoBright, delayMs, randomMode, showFilename, inactivitySleep, themeFlavor, screenOrientation, ledBrightness, isLedEnabled, isWifiEnabled, isMqttEnabled, wifiSSID, wifiPassword, bypassOptimization, bootFromCache);
    
    prefs.end();

    String html = String(settings_html);
    html.replace("%APP_VERSION%", APP_VERSION);
    
    html.replace("%THEME_MOCHA%", themeFlavor == 0 ? "selected" : "");
    html.replace("%THEME_MACCHIATO%", themeFlavor == 1 ? "selected" : "");
    html.replace("%THEME_FRAPPE%", themeFlavor == 2 ? "selected" : "");
    html.replace("%THEME_LATTE%", themeFlavor == 3 ? "selected" : "");
    
    html.replace("%ORIENT_0%", screenOrientation == 0 ? "selected" : "");
    html.replace("%ORIENT_1%", screenOrientation == 1 ? "selected" : "");
    html.replace("%ORIENT_2%", screenOrientation == 2 ? "selected" : "");
    html.replace("%ORIENT_3%", screenOrientation == 3 ? "selected" : "");
    
    html.replace("%BRIGHTNESS%", String(brightness));
    html.replace("%AUTO_BRIGHTNESS%", autoBright ? "checked" : "");
    html.replace("%INACTIVITY_SLEEP%", inactivitySleep ? "checked" : "");
    
    html.replace("%DELAY_SECONDS%", String(delayMs / 1000));
    html.replace("%RANDOM_MODE%", randomMode ? "checked" : "");
    html.replace("%SHOW_FILENAME%", showFilename ? "checked" : "");
    html.replace("%BYPASS_OPT%", bypassOptimization ? "checked" : "");
    html.replace("%BOOT_CACHE%", bootFromCache ? "checked" : "");
    
    html.replace("%LED_ENABLED%", isLedEnabled ? "checked" : "");
    html.replace("%LED_BRIGHTNESS%", String(ledBrightness));
    html.replace("%WIFI_ENABLED%", isWifiEnabled ? "checked" : "");
    html.replace("%MQTT_ENABLED%", isMqttEnabled ? "checked" : "");

    server->send(200, "text/html", html);
}

void WifiManager::handleSettingsSave() {
    WebServer* server = (WebServer*)_webServer;
    Preferences prefs;
    prefs.begin("settings", false);
    
    int brightness = 255;
    bool autoBright = false;
    unsigned long delayMs = 10000;
    bool randomMode = false;
    bool showFilename = false;
    bool inactivitySleep = false;
    int themeFlavor = 0;
    int screenOrientation = 1;
    int ledBrightness = 128;
    bool isLedEnabled = true;
    bool isWifiEnabled = true;
    bool isMqttEnabled = false;
    std::string wifiSSID = "";
    std::string wifiPassword = "";
    bool bypassOptimization = false;
    bool bootFromCache = false;
    
    HardwareLogic::loadSettings(prefs, brightness, autoBright, delayMs, randomMode, showFilename, inactivitySleep, themeFlavor, screenOrientation, ledBrightness, isLedEnabled, isWifiEnabled, isMqttEnabled, wifiSSID, wifiPassword, bypassOptimization, bootFromCache);
    
    if (server->hasArg("theme_flavor")) themeFlavor = server->arg("theme_flavor").toInt();
    if (server->hasArg("screen_orientation")) screenOrientation = server->arg("screen_orientation").toInt();
    if (server->hasArg("brightness")) brightness = server->arg("brightness").toInt();
    
    autoBright = server->hasArg("auto_brightness");
    inactivitySleep = server->hasArg("inactivity_sleep");
    
    if (server->hasArg("slideshow_delay")) delayMs = server->arg("slideshow_delay").toInt() * 1000;
    
    randomMode = server->hasArg("random_mode");
    showFilename = server->hasArg("show_filename");
    bypassOptimization = server->hasArg("bypass_opt");
    bootFromCache = server->hasArg("boot_cache");
    
    isLedEnabled = server->hasArg("led_enabled");
    if (server->hasArg("led_brightness")) ledBrightness = server->arg("led_brightness").toInt();
    
    isWifiEnabled = server->hasArg("wifi_enabled");
    isMqttEnabled = server->hasArg("mqtt_enabled");
    
    HardwareLogic::saveSettings(prefs, brightness, autoBright, delayMs, randomMode, showFilename, inactivitySleep, themeFlavor, screenOrientation, ledBrightness, isLedEnabled, isWifiEnabled, isMqttEnabled, wifiSSID, wifiPassword, bypassOptimization, bootFromCache);
    
    prefs.end();
    
    server->sendHeader("Location", "/", true);
    server->send(302, "text/plain", "Settings saved. Rebooting...");
    
    delay(1000);
    ESP.restart();
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
void WifiManager::handleDashboard() {}
void WifiManager::handleSettings() {}
void WifiManager::handleSettingsSave() {}
#endif
