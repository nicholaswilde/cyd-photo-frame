#ifndef CONFIG_H
#define CONFIG_H

// Slideshow Settings
#define DEFAULT_SLIDESHOW_DELAY_MS 10000
#define DEFAULT_RANDOM_MODE false
#define DEFAULT_SHOW_FILENAME true
#define DEFAULT_BYPASS_OPTIMIZATION false

// Display & Backlight Settings
#define DEFAULT_BRIGHTNESS 255
#define DEFAULT_AUTO_BRIGHTNESS false
#define DEFAULT_INACTIVITY_SLEEP false
#define DEFAULT_SCREEN_ORIENTATION 1
#define DEFAULT_THEME_FLAVOR 0

// Onboard RGB LED Settings
#define DEFAULT_LED_ENABLED true
#define DEFAULT_LED_BRIGHTNESS 128

// Hardware Pin Mapping Overrides
#define LDR_PIN 34
#define RGB_LED_RED_PIN 4
#define RGB_LED_GREEN_PIN 16
#define RGB_LED_BLUE_PIN 17

// SD Card SPI Settings
// #define SD_CS_PIN 5
// #define SD_SCK_PIN 18
// #define SD_MISO_PIN 19
// #define SD_MOSI_PIN 23

// WiFi Settings
#define DEFAULT_WIFI_SSID ""
#define DEFAULT_WIFI_PASSWORD ""

// Static IP Settings
// Uncomment the lines below to configure a static IP address.
// If commented out, the device will use DHCP.
// #define STATIC_IP          "192.168.1.100"
// #define STATIC_GATEWAY     "192.168.1.1"
// #define STATIC_SUBNET      "255.255.255.0"
// #define STATIC_DNS         "1.1.1.1"

#endif // CONFIG_H
