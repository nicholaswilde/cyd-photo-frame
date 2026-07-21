#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// Slideshow Defaults
// =============================================================================

// Delay between slides in milliseconds (default: 10000 = 10s)
#ifndef DEFAULT_SLIDESHOW_DELAY_MS
#define DEFAULT_SLIDESHOW_DELAY_MS 10000
#endif

// Shuffle photos randomly (default: false)
#ifndef DEFAULT_RANDOM_MODE
#define DEFAULT_RANDOM_MODE false
#endif

// Display filename banner overlay at bottom of screen (default: true)
#ifndef DEFAULT_SHOW_FILENAME
#define DEFAULT_SHOW_FILENAME true
#endif

// =============================================================================
// Display & Backlight Defaults
// =============================================================================

// LCD Backlight brightness level: 25 (minimum visible) to 255 (maximum) (default: 255)
#ifndef DEFAULT_BRIGHTNESS
#define DEFAULT_BRIGHTNESS 255
#endif

// Auto-adjust backlight using LDR ambient light sensor (default: false)
#ifndef DEFAULT_AUTO_BRIGHTNESS
#define DEFAULT_AUTO_BRIGHTNESS false
#endif

// Dim screen / sleep after period of touch inactivity (default: false)
#ifndef DEFAULT_INACTIVITY_SLEEP
#define DEFAULT_INACTIVITY_SLEEP false
#endif

// Initial screen orientation (default: 1)
// 1 = Landscape, 2 = Portrait, 3 = Landscape Reverse, 4 = Portrait Reverse
#ifndef DEFAULT_SCREEN_ORIENTATION
#define DEFAULT_SCREEN_ORIENTATION 1
#endif

// Catppuccin UI Theme Flavor (default: 0)
// 0 = Mocha, 1 = Macchiato, 2 = Frappé, 3 = Latte
#ifndef DEFAULT_THEME_FLAVOR
#define DEFAULT_THEME_FLAVOR 0
#endif

// =============================================================================
// Onboard RGB LED Defaults
// =============================================================================

// Onboard RGB LED status (default: true)
#ifndef DEFAULT_LED_ENABLED
#define DEFAULT_LED_ENABLED true
#endif

// Onboard RGB LED brightness: 0 to 255 (default: 128)
#ifndef DEFAULT_LED_BRIGHTNESS
#define DEFAULT_LED_BRIGHTNESS 128
#endif

// =============================================================================
// Hardware Pin Mapping Overrides (uncomment to override defaults)
// =============================================================================

// Ambient Light Sensor (LDR) analog input pin
#ifndef LDR_PIN
#define LDR_PIN 34
#endif

// Onboard RGB LED pins (CYD default: R=4, G=16, B=17, active-LOW)
#ifndef RGB_LED_RED_PIN
#define RGB_LED_RED_PIN 4
#endif

#ifndef RGB_LED_GREEN_PIN
#define RGB_LED_GREEN_PIN 16
#endif

#ifndef RGB_LED_BLUE_PIN
#define RGB_LED_BLUE_PIN 17
#endif

// SD Card SPI Settings
// #define SD_CS_PIN 5
// #define SD_SCK_PIN 18
// #define SD_MISO_PIN 19
// #define SD_MOSI_PIN 23

#endif // CONFIG_H
