#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <string>

enum WifiState {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_MODE
};

class WifiManager {
public:
    WifiManager(const std::string& ssid, const std::string& password);
    void begin();
    void update();
    WifiState getState() const;
    std::string getIPAddress() const;
    std::string getAPSSID() const;

private:
    void startAPMode();
    void handleRoot();
    void handleSave();
    void handleNotFound();
    
    void startScreenshotServer();
    void stopScreenshotServer();
    void handleScreenshot();
    void handleOrientation();
    void handleScreen();

    std::string _ssid;
    std::string _password;
    WifiState _state;
    unsigned long _lastReconnectAttempt;
    unsigned long _connectionStartTime;
    const unsigned long _reconnectInterval = 10000;
    const unsigned long _connectionTimeout = 15000; // 15 seconds
    std::string _cachedNetworksHTML;

#ifndef NATIVE_TEST
    void* _dnsServer = nullptr;
    void* _webServer = nullptr;
#endif
};

#endif // WIFI_MANAGER_H
