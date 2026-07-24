#include "lvgl_manager.h"
#include <stddef.h>
#include <stdlib.h>
#include "catppuccin.h"

extern int currentThemeFlavor;
extern int currentOrientation;

#include "wifi_manager.h"
#include "touch_handler.h"
extern WifiManager* wifiManager;

#if !defined(NATIVE_TEST)
#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "touch_manager.h"
#include "slideshow_timer.h"
#include "app_state.h"
#include "screenshot_manager.h"
#include "led_manager.h"

extern TFT_eSPI tft;

static lv_color_t get_lv_color(uint32_t hex) {
    return lv_color_make((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

// Draw buffer definitions
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;

static void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    // Forward tile to ScreenshotManager if a LVGL capture is in progress
    if (ScreenshotManager::isCaptureInProgress()) {
        ScreenshotManager::onFlushTile(
            area->x1, area->y1, area->x2, area->y2,
            (const uint16_t *)&color_p->full);
    }

    lv_disp_flush_ready(disp_drv);
}

extern TouchHandler touchHandler;

static void (*exitCallback)() = nullptr;
static void (*clearCacheCallback)() = nullptr;
static void (*rebootConfirmCallback)() = nullptr;
static lv_obj_t* settings_screen = nullptr;
static lv_obj_t* confirm_dialog = nullptr;
static lv_obj_t* reboot_confirm_dialog = nullptr;
static lv_obj_t* wifi_info_dialog = nullptr;
static lv_obj_t* slider_bright_ptr = nullptr;
static lv_obj_t* settings_wifi_icon = nullptr;
static lv_obj_t* ap_screen = nullptr;
static lv_obj_t* loading_slideshow_screen = nullptr;

static void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    if (currentState != STATE_SETTINGS && ap_screen == nullptr) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    int touchX = 0, touchY = 0;
    bool touched = TouchManager::isTouched();
    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        TouchManager::getTouchPoint(touchX, touchY);
        int pixelX = 0;
        int pixelY = 0;
        int disp_w = LVGLManager::getWidth();
        int disp_h = LVGLManager::getHeight();
        bool isCapacitive = TouchManager::isCapacitive();
        
        touchHandler.mapCoordinates(touchX, touchY, pixelX, pixelY, isCapacitive);
        
        data->state = LV_INDEV_STATE_PR;
        data->point.x = pixelX;
        data->point.y = pixelY;
    }
}

extern bool isWifiEnabled;
extern bool isMqttEnabled;
extern bool bypassOptimization;
extern int currentBrightness;
extern bool showFilename;
extern bool pendingExitSettings;
extern bool isRandomMode;
extern bool isAutoBrightness;
extern bool isInactivitySleep;
extern SlideshowTimer slideshowTimer;
extern int currentLedBrightness;
extern bool isLedEnabled;
extern LedManager led;

static void brightness_slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    currentBrightness = (int)lv_slider_get_value(slider);
#if defined(TFT_BL) && (TFT_BL >= 0)
    if (!isAutoBrightness) {
        analogWrite(TFT_BL, currentBrightness);
    }
#endif
}

static void led_brightness_slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    currentLedBrightness = (int)lv_slider_get_value(slider);
    led.setBrightness(currentLedBrightness);
}

static void led_enable_switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    isLedEnabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    led.setEnabled(isLedEnabled);
    lv_obj_t * slider = (lv_obj_t *)lv_event_get_user_data(e);
    if (slider) {
        if (isLedEnabled) {
            lv_obj_clear_state(slider, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(slider, LV_STATE_DISABLED);
        }
    }
}

static void auto_brightness_switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    isAutoBrightness = lv_obj_has_state(sw, LV_STATE_CHECKED);
    lv_obj_t * slider = (lv_obj_t *)lv_event_get_user_data(e);
    if (slider) {
        if (isAutoBrightness) {
            lv_obj_add_state(slider, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(slider, LV_STATE_DISABLED);
#if defined(TFT_BL) && (TFT_BL >= 0)
            analogWrite(TFT_BL, currentBrightness);
#endif
        }
    }
}

static void random_mode_switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    isRandomMode = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void show_filename_switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    showFilename = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void inactivity_sleep_switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    isInactivitySleep = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void wifi_switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    isWifiEnabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    lv_obj_t * mqtt_sw = (lv_obj_t *)lv_event_get_user_data(e);
    if (mqtt_sw) {
        if (isWifiEnabled) {
            lv_obj_clear_state(mqtt_sw, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(mqtt_sw, LV_STATE_DISABLED);
            lv_obj_clear_state(mqtt_sw, LV_STATE_CHECKED);
            isMqttEnabled = false;
        }
    }
}

static void mqtt_switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    isMqttEnabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void bypass_optimization_switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    bypassOptimization = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void delay_dropdown_event_cb(lv_event_t * e) {
    lv_obj_t * dropdown = lv_event_get_target(e);
    char buf[16];
    lv_dropdown_get_selected_str(dropdown, buf, sizeof(buf));
    int seconds = atoi(buf);
    if (seconds > 0) {
        slideshowTimer.setInterval((unsigned long)seconds * 1000UL);
    }
}

static void theme_dropdown_event_cb(lv_event_t * e) {
    lv_obj_t * dropdown = lv_event_get_target(e);
    currentThemeFlavor = (int)lv_dropdown_get_selected(dropdown);
}

static const int dropdown_to_rotation[] = {1, 2, 3, 0};
static void orientation_dropdown_event_cb(lv_event_t * e) {
    lv_obj_t * dropdown = lv_event_get_target(e);
    int selected = lv_dropdown_get_selected(dropdown);
    if (selected >= 0 && selected < 4) {
        currentOrientation = dropdown_to_rotation[selected];
    }
}

static void exit_button_event_cb(lv_event_t * e) {
    Serial.println("[LVGL Debug] Save & Exit button pressed.");
    if (exitCallback) {
        exitCallback();
    }
}

static void confirm_clear_cache_cb(lv_event_t * e) {
    if (confirm_dialog) {
        lv_obj_del(confirm_dialog);
        confirm_dialog = nullptr;
    }
    if (clearCacheCallback) {
        clearCacheCallback();
    }
}

static void cancel_clear_cache_cb(lv_event_t * e) {
    if (confirm_dialog) {
        lv_obj_del(confirm_dialog);
        confirm_dialog = nullptr;
    }
}

static void clear_cache_click_event_cb(lv_event_t * e) {
    if (confirm_dialog != nullptr) return;

    confirm_dialog = lv_obj_create(settings_screen);
    lv_obj_set_size(confirm_dialog, LVGLManager::getWidth(), LVGLManager::getHeight());
    lv_obj_align(confirm_dialog, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(confirm_dialog, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).base), 0);
    lv_obj_set_style_border_width(confirm_dialog, 0, 0);
    lv_obj_clear_flag(confirm_dialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * lbl_title = lv_label_create(confirm_dialog);
    lv_label_set_text(lbl_title, "CYD Photo Frame");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 15);

    lv_obj_t * lbl_subtitle = lv_label_create(confirm_dialog);
    lv_label_set_text(lbl_subtitle, "Clear Cache?");
    lv_obj_set_style_text_color(lbl_subtitle, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(lbl_subtitle, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t * lbl_msg = lv_label_create(confirm_dialog);
    lv_label_set_text(lbl_msg, "Delete all cached photos?\nThey will be re-generated.");
    lv_obj_set_style_text_align(lbl_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_msg, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(lbl_msg, LV_ALIGN_TOP_MID, 0, 80);

    // Confirm button
    lv_obj_t * btn_confirm = lv_btn_create(confirm_dialog);
    lv_obj_set_size(btn_confirm, 90, 32);
    lv_obj_align(btn_confirm, LV_ALIGN_BOTTOM_LEFT, 20, -30);
    lv_obj_set_style_bg_color(btn_confirm, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).red), 0);
    lv_obj_add_event_cb(btn_confirm, confirm_clear_cache_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(lbl_confirm, "Confirm");
    lv_obj_set_style_text_color(lbl_confirm, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).crust), 0);
    lv_obj_align(lbl_confirm, LV_ALIGN_CENTER, 0, 0);

    // Cancel button
    lv_obj_t * btn_cancel = lv_btn_create(confirm_dialog);
    lv_obj_set_size(btn_cancel, 90, 32);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_RIGHT, -20, -30);
    lv_obj_set_style_bg_color(btn_cancel, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).overlay), 0);
    lv_obj_add_event_cb(btn_cancel, cancel_clear_cache_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "Cancel");
    lv_obj_set_style_text_color(lbl_cancel, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).crust), 0);
    lv_obj_align(lbl_cancel, LV_ALIGN_CENTER, 0, 0);
}

static void confirm_reboot_cb(lv_event_t * e) {
    if (reboot_confirm_dialog) {
        lv_obj_del(reboot_confirm_dialog);
        reboot_confirm_dialog = nullptr;
    }
    if (rebootConfirmCallback) {
        rebootConfirmCallback();
    }
}

static void cancel_reboot_cb(lv_event_t * e) {
    if (reboot_confirm_dialog) {
        lv_obj_del(reboot_confirm_dialog);
        reboot_confirm_dialog = nullptr;
    }
}

void LVGLManager::showRebootConfirmDialog() {
    if (reboot_confirm_dialog != nullptr) return;

    reboot_confirm_dialog = lv_obj_create(settings_screen);
    lv_obj_set_size(reboot_confirm_dialog, LVGLManager::getWidth(), LVGLManager::getHeight());
    lv_obj_align(reboot_confirm_dialog, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(reboot_confirm_dialog, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).base), 0);
    lv_obj_set_style_border_width(reboot_confirm_dialog, 0, 0);
    lv_obj_clear_flag(reboot_confirm_dialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * lbl_title = lv_label_create(reboot_confirm_dialog);
    lv_label_set_text(lbl_title, "CYD Photo Frame");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 15);

    lv_obj_t * lbl_subtitle = lv_label_create(reboot_confirm_dialog);
    lv_label_set_text(lbl_subtitle, "Reboot Required");
    lv_obj_set_style_text_color(lbl_subtitle, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(lbl_subtitle, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t * lbl_msg = lv_label_create(reboot_confirm_dialog);
    lv_label_set_text(lbl_msg, "Settings changed. Photos\nwill be optimized again.");
    lv_obj_set_style_text_align(lbl_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_msg, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(lbl_msg, LV_ALIGN_TOP_MID, 0, 80);

    // Confirm button
    lv_obj_t * btn_confirm = lv_btn_create(reboot_confirm_dialog);
    lv_obj_set_size(btn_confirm, 90, 32);
    lv_obj_align(btn_confirm, LV_ALIGN_BOTTOM_LEFT, 20, -30);
    lv_obj_set_style_bg_color(btn_confirm, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).red), 0);
    lv_obj_add_event_cb(btn_confirm, confirm_reboot_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(lbl_confirm, "Reboot");
    lv_obj_set_style_text_color(lbl_confirm, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).crust), 0);
    lv_obj_align(lbl_confirm, LV_ALIGN_CENTER, 0, 0);

    // Cancel button
    lv_obj_t * btn_cancel = lv_btn_create(reboot_confirm_dialog);
    lv_obj_set_size(btn_cancel, 90, 32);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_RIGHT, -20, -30);
    lv_obj_set_style_bg_color(btn_cancel, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).overlay), 0);
    lv_obj_add_event_cb(btn_cancel, cancel_reboot_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "Cancel");
    lv_obj_set_style_text_color(lbl_cancel, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).crust), 0);
    lv_obj_align(lbl_cancel, LV_ALIGN_CENTER, 0, 0);
}
#endif

static bool initialized = false;
static int disp_width = 0;
static int disp_height = 0;

void LVGLManager::init(int width, int height) {
    disp_width = width;
    disp_height = height;
    
#if !defined(NATIVE_TEST)
    lv_init();
    
    if (buf1 == nullptr) {
        buf1 = (lv_color_t*)malloc(width * 10 * sizeof(lv_color_t));
    }
    
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, width * 10);
    
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = width;
    disp_drv.ver_res = height;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
#endif

    initialized = true;
}

void LVGLManager::handle() {
#if !defined(NATIVE_TEST)
    if (initialized) {
        lv_timer_handler();
        
        // If settings menu is open and auto-brightness is active, keep slider in sync
        if (settings_screen && lv_scr_act() == settings_screen) {
            if (isAutoBrightness && slider_bright_ptr) {
                lv_slider_set_value(slider_bright_ptr, currentBrightness, LV_ANIM_OFF);
            }
            if (settings_wifi_icon) {
                if (wifiManager == nullptr) {
                    lv_obj_add_flag(settings_wifi_icon, LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_clear_flag(settings_wifi_icon, LV_OBJ_FLAG_HIDDEN);
                    WifiState ws = wifiManager->getState();
                    if (ws == WIFI_STATE_CONNECTED) {
                        lv_obj_set_style_text_color(settings_wifi_icon, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).green), 0);
                    } else if (ws == WIFI_STATE_CONNECTING || ws == WIFI_STATE_AP_MODE) {
                        lv_obj_set_style_text_color(settings_wifi_icon, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).yellow), 0);
                    } else {
                        lv_obj_set_style_text_color(settings_wifi_icon, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).red), 0);
                    }
                }
            }
        }
    }
#endif
}

bool LVGLManager::isInitialized() {
    return initialized;
}

int LVGLManager::getWidth() {
    return disp_width;
}

int LVGLManager::getHeight() {
    return disp_height;
}

void LVGLManager::setExitCallback(void (*exit_cb)()) {
#if !defined(NATIVE_TEST)
    exitCallback = exit_cb;
#endif
}

void LVGLManager::setRebootConfirmCallback(void (*reboot_cb)()) {
#if !defined(NATIVE_TEST)
    rebootConfirmCallback = reboot_cb;
#endif
}

void LVGLManager::setClearCacheCallback(void (*clear_cache_cb)()) {
#if !defined(NATIVE_TEST)
    clearCacheCallback = clear_cache_cb;
#endif
}

#if !defined(NATIVE_TEST)
static void close_wifi_info_cb(lv_event_t * e) {
    if (wifi_info_dialog) {
        lv_obj_del(wifi_info_dialog);
        wifi_info_dialog = nullptr;
    }
}

static void wifi_icon_click_cb(lv_event_t * e) {
    if (wifiManager == nullptr) return;
    if (wifiManager->getState() != WIFI_STATE_CONNECTED) return;
    if (wifi_info_dialog != nullptr) return;

    wifi_info_dialog = lv_obj_create(settings_screen);
    lv_obj_set_size(wifi_info_dialog, LVGLManager::getWidth(), LVGLManager::getHeight());
    lv_obj_align(wifi_info_dialog, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(wifi_info_dialog, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).base), 0);
    lv_obj_set_style_border_width(wifi_info_dialog, 0, 0);
    lv_obj_clear_flag(wifi_info_dialog, LV_OBJ_FLAG_SCROLLABLE);

    // Title label
    lv_obj_t * lbl_title = lv_label_create(wifi_info_dialog);
    lv_label_set_text(lbl_title, "WiFi Info");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 15);

    // Status label
    lv_obj_t * lbl_status = lv_label_create(wifi_info_dialog);
    lv_label_set_text(lbl_status, "Connected");
    lv_obj_set_style_text_color(lbl_status, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).green), 0);
    lv_obj_align(lbl_status, LV_ALIGN_TOP_MID, 0, 50);

    // Info details
    lv_obj_t * lbl_info = lv_label_create(wifi_info_dialog);
    char infoBuf[256];
    snprintf(infoBuf, sizeof(infoBuf), 
             "SSID: %s\nIP: %s\nMAC: %s\nRSSI: %d dBm", 
             WiFi.SSID().c_str(), 
             WiFi.localIP().toString().c_str(), 
             WiFi.macAddress().c_str(), 
             WiFi.RSSI());
    lv_label_set_text(lbl_info, infoBuf);
    lv_obj_set_style_text_align(lbl_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_info, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(lbl_info, LV_ALIGN_TOP_MID, 0, 80);

    // Close Button
    lv_obj_t * btn_close = lv_btn_create(wifi_info_dialog);
    lv_obj_set_size(btn_close, 100, 32);
    lv_obj_align(btn_close, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_color(btn_close, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).overlay), 0);
    lv_obj_add_event_cb(btn_close, close_wifi_info_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, "Close");
    lv_obj_set_style_text_color(lbl_close, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).crust), 0);
    lv_obj_align(lbl_close, LV_ALIGN_CENTER, 0, 0);
}
#endif

void LVGLManager::showSettings() {
#if !defined(NATIVE_TEST)
    if (settings_screen != nullptr) {
        return; // Already showing
    }

    settings_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(settings_screen, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).base), 0);
    
    lv_obj_t * title = lv_label_create(settings_screen);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // WiFi status icon in top right
    settings_wifi_icon = lv_label_create(settings_screen);
    lv_label_set_text(settings_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_align(settings_wifi_icon, LV_ALIGN_TOP_RIGHT, -15, 15);
    lv_obj_add_flag(settings_wifi_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(settings_wifi_icon, 15);
    lv_obj_add_event_cb(settings_wifi_icon, wifi_icon_click_cb, LV_EVENT_CLICKED, NULL);
    if (wifiManager == nullptr) {
        lv_obj_add_flag(settings_wifi_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        WifiState ws = wifiManager->getState();
        if (ws == WIFI_STATE_CONNECTED) {
            lv_obj_set_style_text_color(settings_wifi_icon, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).green), 0);
        } else if (ws == WIFI_STATE_CONNECTING || ws == WIFI_STATE_AP_MODE) {
            lv_obj_set_style_text_color(settings_wifi_icon, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).yellow), 0);
        } else {
            lv_obj_set_style_text_color(settings_wifi_icon, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).red), 0);
        }
    }
    
    lv_obj_t * list = lv_obj_create(settings_screen);
    lv_obj_set_size(list, LV_PCT(95), LV_PCT(74));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(list, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).base), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 5, 0);
    lv_obj_set_style_pad_row(list, 10, 0);
    lv_obj_set_style_pad_bottom(list, 20, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    
    // 1. Brightness Slider
    lv_obj_t * row_bright = lv_obj_create(list);
    lv_obj_clear_flag(row_bright, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row_bright, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_bright, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_bright, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_bright, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).mantle), 0);
    lv_obj_set_style_border_width(row_bright, 0, 0);
    lv_obj_set_style_pad_all(row_bright, 5, 0);

    lv_obj_t * lbl_bright = lv_label_create(row_bright);
    lv_label_set_text(lbl_bright, "Brightness");
    lv_obj_set_style_text_color(lbl_bright, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);

    lv_obj_t * slider_bright = lv_slider_create(row_bright);
    slider_bright_ptr = slider_bright;
    lv_obj_set_size(slider_bright, 80, 10);
    lv_obj_set_style_pad_right(row_bright, 15, 0);
    lv_slider_set_range(slider_bright, 25, 255);
    lv_slider_set_value(slider_bright, currentBrightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_bright, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // 2. Auto Brightness
    lv_obj_t * row_auto = lv_obj_create(list);
    lv_obj_clear_flag(row_auto, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row_auto, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_auto, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_auto, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_auto, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).mantle), 0);
    lv_obj_set_style_border_width(row_auto, 0, 0);
    lv_obj_set_style_pad_all(row_auto, 5, 0);

    lv_obj_t * lbl_auto = lv_label_create(row_auto);
    lv_label_set_text(lbl_auto, "Auto Brightness (LDR)");
    lv_obj_set_style_text_color(lbl_auto, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);

    lv_obj_t * sw_auto = lv_switch_create(row_auto);
    if (isAutoBrightness) {
        lv_obj_add_state(sw_auto, LV_STATE_CHECKED);
        lv_obj_add_state(slider_bright, LV_STATE_DISABLED);
    }
    lv_obj_add_event_cb(sw_auto, auto_brightness_switch_event_cb, LV_EVENT_VALUE_CHANGED, slider_bright);
    
    // 2a. LED Brightness Slider
    lv_obj_t * row_led = lv_obj_create(list);
    lv_obj_clear_flag(row_led, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row_led, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_led, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_led, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_led, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).mantle), 0);
    lv_obj_set_style_border_width(row_led, 0, 0);
    lv_obj_set_style_pad_all(row_led, 5, 0);

    lv_obj_t * lbl_led = lv_label_create(row_led);
    lv_label_set_text(lbl_led, "LED Brightness");
    lv_obj_set_style_text_color(lbl_led, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);

    lv_obj_t * slider_led = lv_slider_create(row_led);
    lv_obj_set_size(slider_led, 80, 10);
    lv_obj_set_style_pad_right(row_led, 15, 0);
    lv_slider_set_range(slider_led, 0, 255);
    lv_slider_set_value(slider_led, currentLedBrightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_led, led_brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 2b. Enable LED Switch
    lv_obj_t * row_led_enable = lv_obj_create(list);
    lv_obj_clear_flag(row_led_enable, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row_led_enable, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_led_enable, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_led_enable, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_led_enable, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).mantle), 0);
    lv_obj_set_style_border_width(row_led_enable, 0, 0);
    lv_obj_set_style_pad_all(row_led_enable, 5, 0);

    lv_obj_t * lbl_led_enable = lv_label_create(row_led_enable);
    lv_label_set_text(lbl_led_enable, "LED Light");
    lv_obj_set_style_text_color(lbl_led_enable, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);

    lv_obj_t * sw_led_enable = lv_switch_create(row_led_enable);
    if (isLedEnabled) {
        lv_obj_add_state(sw_led_enable, LV_STATE_CHECKED);
    } else {
        lv_obj_add_state(slider_led, LV_STATE_DISABLED);
    }
    lv_obj_add_event_cb(sw_led_enable, led_enable_switch_event_cb, LV_EVENT_VALUE_CHANGED, slider_led);

    // 3. Delay Dropdown
    lv_obj_t * row_delay = lv_obj_create(list);
    lv_obj_clear_flag(row_delay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row_delay, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_delay, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_delay, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_delay, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).mantle), 0);
    lv_obj_set_style_border_width(row_delay, 0, 0);
    lv_obj_set_style_pad_all(row_delay, 5, 0);

    lv_obj_t * lbl_delay = lv_label_create(row_delay);
    lv_label_set_text(lbl_delay, "Delay");
    lv_obj_set_style_text_color(lbl_delay, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);

    lv_obj_t * dd_delay = lv_dropdown_create(row_delay);
    lv_dropdown_set_options(dd_delay, "2s\n5s\n10s\n15s\n30s\n60s");
    
    unsigned long interval_sec = slideshowTimer.getInterval() / 1000UL;
    int sel_idx = 2; // Default 10s
    if (interval_sec == 2) sel_idx = 0;
    else if (interval_sec == 5) sel_idx = 1;
    else if (interval_sec == 10) sel_idx = 2;
    else if (interval_sec == 15) sel_idx = 3;
    else if (interval_sec == 30) sel_idx = 4;
    else if (interval_sec == 60) sel_idx = 5;
    lv_dropdown_set_selected(dd_delay, sel_idx);
    lv_obj_add_event_cb(dd_delay, delay_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // 4. Random Mode
    lv_obj_t * row_random = lv_obj_create(list);
    lv_obj_clear_flag(row_random, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row_random, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_random, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_random, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_random, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).mantle), 0);
    lv_obj_set_style_border_width(row_random, 0, 0);
    lv_obj_set_style_pad_all(row_random, 5, 0);

    lv_obj_t * lbl_random = lv_label_create(row_random);
    lv_label_set_text(lbl_random, "Random Mode");
    lv_obj_set_style_text_color(lbl_random, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);

    lv_obj_t * sw_random = lv_switch_create(row_random);
    if (isRandomMode) lv_obj_add_state(sw_random, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_random, random_mode_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // 5. Show Filename
    lv_obj_t * row_filename = lv_obj_create(list);
    lv_obj_clear_flag(row_filename, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row_filename, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_filename, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_filename, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_filename, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).mantle), 0);
    lv_obj_set_style_border_width(row_filename, 0, 0);
    lv_obj_set_style_pad_all(row_filename, 5, 0);

    lv_obj_t * lbl_filename = lv_label_create(row_filename);
    lv_label_set_text(lbl_filename, "Show Filename");
    lv_obj_set_style_text_color(lbl_filename, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);

    lv_obj_t * sw_filename = lv_switch_create(row_filename);
    if (showFilename) lv_obj_add_state(sw_filename, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_filename, show_filename_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // 6. Inactivity Sleep
    lv_obj_t * row_sleep = lv_obj_create(list);
    lv_obj_clear_flag(row_sleep, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row_sleep, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_sleep, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_sleep, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_sleep, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).mantle), 0);
    lv_obj_set_style_border_width(row_sleep, 0, 0);
    lv_obj_set_style_pad_all(row_sleep, 5, 0);

    lv_obj_t * lbl_sleep = lv_label_create(row_sleep);
    lv_label_set_text(lbl_sleep, "Inactivity Sleep");
    lv_obj_set_style_text_color(lbl_sleep, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);

    lv_obj_t * sw_sleep = lv_switch_create(row_sleep);
    if (isInactivitySleep) lv_obj_add_state(sw_sleep, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_sleep, inactivity_sleep_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 6b. Enable WiFi Switch
    lv_obj_t * row_wifi = lv_obj_create(list);
    lv_obj_clear_flag(row_wifi, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row_wifi, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_wifi, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_wifi, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_wifi, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).mantle), 0);
    lv_obj_set_style_border_width(row_wifi, 0, 0);
    lv_obj_set_style_pad_all(row_wifi, 5, 0);

    lv_obj_t * lbl_wifi = lv_label_create(row_wifi);
    lv_label_set_text(lbl_wifi, "WiFi");
    lv_obj_set_style_text_color(lbl_wifi, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);

    lv_obj_t * sw_wifi = lv_switch_create(row_wifi);
    if (isWifiEnabled) {
        lv_obj_add_state(sw_wifi, LV_STATE_CHECKED);
    }

    // 6c. Enable MQTT Switch
    lv_obj_t * row_mqtt = lv_obj_create(list);
    lv_obj_clear_flag(row_mqtt, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row_mqtt, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_mqtt, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_mqtt, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_mqtt, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).mantle), 0);
    lv_obj_set_style_border_width(row_mqtt, 0, 0);
    lv_obj_set_style_pad_all(row_mqtt, 5, 0);

    lv_obj_t * lbl_mqtt = lv_label_create(row_mqtt);
    lv_label_set_text(lbl_mqtt, "MQTT");
    lv_obj_set_style_text_color(lbl_mqtt, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);

    lv_obj_t * sw_mqtt = lv_switch_create(row_mqtt);
    if (isMqttEnabled) {
        lv_obj_add_state(sw_mqtt, LV_STATE_CHECKED);
    }
    if (!isWifiEnabled) {
        lv_obj_add_state(sw_mqtt, LV_STATE_DISABLED);
    }
    lv_obj_add_event_cb(sw_mqtt, mqtt_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Attach event callback to WiFi switch, passing MQTT switch as user_data
    lv_obj_add_event_cb(sw_wifi, wifi_switch_event_cb, LV_EVENT_VALUE_CHANGED, sw_mqtt);

    // 6d. Bypass Optimization Switch
    lv_obj_t * row_bypass_opt = lv_obj_create(list);
    lv_obj_clear_flag(row_bypass_opt, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row_bypass_opt, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_bypass_opt, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_bypass_opt, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_bypass_opt, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).mantle), 0);
    lv_obj_set_style_border_width(row_bypass_opt, 0, 0);
    lv_obj_set_style_pad_all(row_bypass_opt, 5, 0);

    lv_obj_t * lbl_bypass_opt = lv_label_create(row_bypass_opt);
    lv_label_set_text(lbl_bypass_opt, "Bypass Optimization");
    lv_obj_set_style_text_color(lbl_bypass_opt, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);

    lv_obj_t * sw_bypass_opt = lv_switch_create(row_bypass_opt);
    if (bypassOptimization) {
        lv_obj_add_state(sw_bypass_opt, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw_bypass_opt, bypass_optimization_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 7. Theme Flavor Dropdown
    lv_obj_t * row_theme = lv_obj_create(list);
    lv_obj_clear_flag(row_theme, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row_theme, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_theme, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_theme, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_theme, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).mantle), 0);
    lv_obj_set_style_border_width(row_theme, 0, 0);
    lv_obj_set_style_pad_all(row_theme, 5, 0);

    lv_obj_t * lbl_theme = lv_label_create(row_theme);
    lv_label_set_text(lbl_theme, "Theme");
    lv_obj_set_style_text_color(lbl_theme, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);

    lv_obj_t * dd_theme = lv_dropdown_create(row_theme);
    lv_dropdown_set_options(dd_theme, "Mocha\nMacchiato\nFrappe\nLatte");
    lv_dropdown_set_selected(dd_theme, currentThemeFlavor);
    lv_obj_add_event_cb(dd_theme, theme_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 8. Screen Orientation Dropdown
    lv_obj_t * row_orient = lv_obj_create(list);
    lv_obj_clear_flag(row_orient, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row_orient, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_orient, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_orient, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_orient, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).mantle), 0);
    lv_obj_set_style_border_width(row_orient, 0, 0);
    lv_obj_set_style_pad_all(row_orient, 5, 0);

    lv_obj_t * lbl_orient = lv_label_create(row_orient);
    lv_label_set_text(lbl_orient, "Orientation");
    lv_obj_set_style_text_color(lbl_orient, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);

    static const int rotation_to_dropdown[] = {3, 0, 1, 2};
    lv_obj_t * dd_orient = lv_dropdown_create(row_orient);
    lv_dropdown_set_options(dd_orient, "Landscape\nPortrait\nLandscape Rev\nPortrait Rev");
    int initial_dd = 0;
    if (currentOrientation >= 0 && currentOrientation < 4) {
        initial_dd = rotation_to_dropdown[currentOrientation];
    }
    lv_dropdown_set_selected(dd_orient, initial_dd);
    lv_obj_add_event_cb(dd_orient, orientation_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 8b. Storage / Clear Cache
    lv_obj_t * row_cache = lv_obj_create(list);
    lv_obj_clear_flag(row_cache, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row_cache, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_cache, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_cache, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_cache, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).mantle), 0);
    lv_obj_set_style_border_width(row_cache, 0, 0);
    lv_obj_set_style_pad_all(row_cache, 5, 0);

    lv_obj_t * lbl_cache = lv_label_create(row_cache);
    lv_label_set_text(lbl_cache, "Storage");
    lv_obj_set_style_text_color(lbl_cache, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);

    lv_obj_t * btn_cache = lv_btn_create(row_cache);
    lv_obj_set_size(btn_cache, 90, 28);
    lv_obj_set_style_bg_color(btn_cache, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).red), 0);
    lv_obj_set_style_pad_all(btn_cache, 0, 0);
    
    lv_obj_t * lbl_btn_cache = lv_label_create(btn_cache);
    lv_label_set_text(lbl_btn_cache, "Clear Cache");
    lv_obj_set_style_text_color(lbl_btn_cache, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).crust), 0);
    lv_obj_align(lbl_btn_cache, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(btn_cache, clear_cache_click_event_cb, LV_EVENT_CLICKED, NULL);

    // 9. Save & Exit Button
    lv_obj_t * btn_exit = lv_btn_create(settings_screen);
    lv_obj_set_size(btn_exit, 100, 30);
    lv_obj_align(btn_exit, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(btn_exit, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).green), 0);
    
    lv_obj_t * lbl_exit = lv_label_create(btn_exit);
    lv_label_set_text(lbl_exit, "Save & Exit");
    lv_obj_set_style_text_color(lbl_exit, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).crust), 0);
    lv_obj_align(lbl_exit, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_add_event_cb(btn_exit, exit_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_scr_load(settings_screen);
#endif
}

void LVGLManager::hideSettings() {
#if !defined(NATIVE_TEST)
    if (settings_screen != nullptr) {
        lv_obj_t * old_scr = settings_screen;
        lv_obj_t * blank_scr = lv_obj_create(NULL);
        lv_obj_set_style_bg_opa(blank_scr, LV_OPA_TRANSP, 0);
        lv_scr_load(blank_scr);
        lv_obj_del(old_scr);
        settings_screen = nullptr;
        slider_bright_ptr = nullptr;
        settings_wifi_icon = nullptr;
    }
#endif
}

#if !defined(NATIVE_TEST)
static lv_obj_t * opt_screen = nullptr;
static lv_obj_t * opt_lbl_status = nullptr;
static lv_obj_t * opt_lbl_file = nullptr;
static lv_obj_t * opt_lbl_percent = nullptr;
static lv_obj_t * opt_bar = nullptr;
static lv_obj_t * opt_btn_cancel = nullptr;
static void (*opt_cancel_callback)() = nullptr;

static void opt_cancel_btn_cb(lv_event_t * e) {
    Serial.println("[LVGL Debug] Cancel button event triggered!");
    if (opt_cancel_callback) {
        opt_cancel_callback();
    }
}
#else
static void (*opt_cancel_callback)() = nullptr;
#endif

void LVGLManager::setCancelCallback(void (*cancel_cb)()) {
    opt_cancel_callback = cancel_cb;
}

void LVGLManager::showSDError() {
#if !defined(NATIVE_TEST)
    lv_obj_t * sd_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(sd_screen, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).base), 0);
    lv_obj_set_style_bg_opa(sd_screen, LV_OPA_COVER, 0);

    lv_obj_t * lbl_title = lv_label_create(sd_screen);
    lv_label_set_text(lbl_title, "CYD Photo Frame");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 15);

    lv_obj_t * lbl_err = lv_label_create(sd_screen);
    lv_label_set_text(lbl_err, "NO SD CARD");
    lv_obj_set_style_text_color(lbl_err, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).red), 0);
    lv_obj_align(lbl_err, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t * lbl_inst = lv_label_create(sd_screen);
    lv_label_set_text(lbl_inst, "Insert card and reboot");
    lv_obj_set_style_text_color(lbl_inst, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(lbl_inst, LV_ALIGN_TOP_MID, 0, 80);

    lv_scr_load(sd_screen);
    lv_task_handler();
#endif
}

void LVGLManager::showNoPhotosWarning() {
#if !defined(NATIVE_TEST)
    lv_obj_t * warn_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(warn_screen, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).base), 0);
    lv_obj_set_style_bg_opa(warn_screen, LV_OPA_COVER, 0);

    lv_obj_t * lbl_title = lv_label_create(warn_screen);
    lv_label_set_text(lbl_title, "CYD Photo Frame");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 15);

    lv_obj_t * lbl_err = lv_label_create(warn_screen);
    lv_label_set_text(lbl_err, "NO PHOTOS FOUND");
    lv_obj_set_style_text_color(lbl_err, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).yellow), 0);
    lv_obj_align(lbl_err, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t * lbl_inst = lv_label_create(warn_screen);
    lv_label_set_text(lbl_inst, "Add JPEGs to SD card\nand reboot device.");
    lv_obj_set_style_text_color(lbl_inst, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_set_style_text_align(lbl_inst, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_inst, LV_ALIGN_TOP_MID, 0, 80);

    lv_scr_load(warn_screen);
    lv_task_handler();
#endif
}

void LVGLManager::showLoadingSlideshowScreen(const char* message, bool isError) {
#if !defined(NATIVE_TEST)
    if (loading_slideshow_screen != nullptr) {
        lv_obj_del(loading_slideshow_screen);
        loading_slideshow_screen = nullptr;
    }

    loading_slideshow_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(loading_slideshow_screen, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).base), 0);
    lv_obj_set_style_bg_opa(loading_slideshow_screen, LV_OPA_COVER, 0);

    // Title label
    lv_obj_t * lbl_title = lv_label_create(loading_slideshow_screen);
    lv_label_set_text(lbl_title, "CYD Photo Frame");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 15);

    // Message label
    lv_obj_t * lbl_msg = lv_label_create(loading_slideshow_screen);
    lv_label_set_text(lbl_msg, message);
    if (isError) {
        lv_obj_set_style_text_color(lbl_msg, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).red), 0);
    } else {
        lv_obj_set_style_text_color(lbl_msg, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).green), 0);
    }
    // Match the vertical position of the optimize photos screen
    lv_obj_align(lbl_msg, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_text_align(lbl_msg, LV_TEXT_ALIGN_CENTER, 0);

    lv_scr_load(loading_slideshow_screen);
    lv_task_handler();
#endif
}

void LVGLManager::hideLoadingSlideshowScreen() {
#if !defined(NATIVE_TEST)
    if (loading_slideshow_screen != nullptr) {
        lv_obj_t * old_scr = loading_slideshow_screen;
        lv_obj_t * blank_scr = lv_obj_create(NULL);
        lv_obj_set_style_bg_opa(blank_scr, LV_OPA_TRANSP, 0);
        lv_scr_load(blank_scr);
        lv_obj_del(old_scr);
        loading_slideshow_screen = nullptr;
    }
#endif
}

void LVGLManager::showOptimizationScreen() {
#if !defined(NATIVE_TEST)
    if (opt_screen != nullptr) return;

    opt_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(opt_screen, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).base), 0);
    lv_obj_set_style_bg_opa(opt_screen, LV_OPA_COVER, 0);
    lv_obj_add_flag(opt_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(opt_screen, opt_cancel_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(opt_screen, opt_cancel_btn_cb, LV_EVENT_PRESSED, NULL);

    // Title label
    lv_obj_t * lbl_title = lv_label_create(opt_screen);
    lv_label_set_text(lbl_title, "CYD Photo Frame");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 15);

    // Subtitle label
    opt_lbl_status = lv_label_create(opt_screen);
    lv_label_set_text(opt_lbl_status, "Optimizing Photos...");
    lv_obj_set_style_text_color(opt_lbl_status, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(opt_lbl_status, LV_ALIGN_TOP_MID, 0, 50);

    // Filename / Status detail label
    opt_lbl_file = lv_label_create(opt_screen);
    lv_label_set_text(opt_lbl_file, "Analyzing SD Card...");
    lv_obj_set_style_text_color(opt_lbl_file, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).blue), 0);
    lv_obj_align(opt_lbl_file, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_text_align(opt_lbl_file, LV_TEXT_ALIGN_CENTER, 0);

    // Progress Bar
    opt_bar = lv_bar_create(opt_screen);
    lv_obj_set_size(opt_bar, LVGLManager::getWidth() - 80, 20);
    lv_obj_align(opt_bar, LV_ALIGN_TOP_MID, 0, 130);
    lv_obj_set_style_bg_color(opt_bar, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).overlay), 0);
    lv_obj_set_style_bg_color(opt_bar, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).green), LV_PART_INDICATOR);
    lv_bar_set_value(opt_bar, 0, LV_ANIM_OFF);

    // Percentage / Counter label
    opt_lbl_percent = lv_label_create(opt_screen);
    lv_label_set_text(opt_lbl_percent, "Calculating...");
    lv_obj_set_style_text_color(opt_lbl_percent, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(opt_lbl_percent, LV_ALIGN_TOP_MID, 0, 160);
    lv_obj_set_style_text_align(opt_lbl_percent, LV_TEXT_ALIGN_CENTER, 0);

    // Cancel Button
    opt_btn_cancel = lv_btn_create(opt_screen);
    lv_obj_set_size(opt_btn_cancel, 100, 32);
    lv_obj_align(opt_btn_cancel, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_color(opt_btn_cancel, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).red), 0);
    lv_obj_add_event_cb(opt_btn_cancel, opt_cancel_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(opt_btn_cancel, opt_cancel_btn_cb, LV_EVENT_PRESSED, NULL);

    lv_obj_t * lbl_cancel = lv_label_create(opt_btn_cancel);
    lv_label_set_text(lbl_cancel, "Cancel");
    lv_obj_set_style_text_color(lbl_cancel, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).crust), 0);
    lv_obj_align(lbl_cancel, LV_ALIGN_CENTER, 0, 0);

    lv_scr_load(opt_screen);
    lv_task_handler();
#endif
}

void LVGLManager::updateCalculationProgress(size_t current, size_t total, const char* filename, size_t needsOptCount) {
#if !defined(NATIVE_TEST)
    if (opt_screen == nullptr) showOptimizationScreen();

    if (opt_lbl_status) {
        lv_label_set_text(opt_lbl_status, "Scanning SD Card...");
    }

    if (opt_lbl_file && filename) {
        char fileStr[128];
        snprintf(fileStr, sizeof(fileStr), "Checking:\n%s", filename);
        lv_label_set_text(opt_lbl_file, fileStr);
    }

    int percentage = (total == 0) ? 0 : (current * 100) / total;
    if (opt_bar) {
        lv_bar_set_value(opt_bar, percentage, LV_ANIM_OFF);
    }

    if (opt_lbl_percent) {
        char percentStr[64];
        if (needsOptCount == 1) {
            snprintf(percentStr, sizeof(percentStr), "%d%% (%zu/%zu)\n1 needs optimization", percentage, current, total);
        } else {
            snprintf(percentStr, sizeof(percentStr), "%d%% (%zu/%zu)\n%zu need optimization", percentage, current, total, needsOptCount);
        }
        lv_label_set_text(opt_lbl_percent, percentStr);
    }

    lv_task_handler();
#endif
}

void LVGLManager::updateOptimizationProgress(size_t current, size_t total, const char* filename) {
#if !defined(NATIVE_TEST)
    if (opt_screen == nullptr) showOptimizationScreen();

    if (opt_lbl_status) {
        lv_label_set_text(opt_lbl_status, "Optimizing Photos...");
    }

    if (opt_lbl_file && filename) {
        char fileStr[128];
        snprintf(fileStr, sizeof(fileStr), "Optimizing:\n%s", filename);
        lv_label_set_text(opt_lbl_file, fileStr);
    }

    int percentage = (total == 0) ? 0 : (current * 100) / total;
    if (opt_bar) {
        lv_bar_set_value(opt_bar, percentage, LV_ANIM_OFF);
    }

    if (opt_lbl_percent) {
        char percentStr[64];
        snprintf(percentStr, sizeof(percentStr), "Optimizing %zu/%zu (%d%%)", current, total, percentage);
        lv_label_set_text(opt_lbl_percent, percentStr);
    }

    lv_task_handler();
#endif
}

void LVGLManager::setOptimizationCancelling() {
#if !defined(NATIVE_TEST)
    if (opt_lbl_percent) {
        lv_label_set_text(opt_lbl_percent, "Cancelling...");
        lv_obj_set_style_text_color(opt_lbl_percent, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).red), 0);
    }
    lv_task_handler();
#endif
}

void LVGLManager::hideOptimizationScreen() {
#if !defined(NATIVE_TEST)
    if (opt_screen != nullptr) {
        lv_obj_t * old_scr = opt_screen;
        lv_obj_t * blank_scr = lv_obj_create(NULL);
        lv_obj_set_style_bg_opa(blank_scr, LV_OPA_TRANSP, 0);
        lv_scr_load(blank_scr);
        lv_obj_del(old_scr);
        opt_screen = nullptr;
        opt_lbl_status = nullptr;
        opt_lbl_file = nullptr;
        opt_lbl_percent = nullptr;
        opt_bar = nullptr;
        opt_btn_cancel = nullptr;
    }
#endif
}

void LVGLManager::showClearCacheScreen() {
#if !defined(NATIVE_TEST)
    if (opt_screen != nullptr) return;

    showOptimizationScreen();

    if (opt_lbl_status) {
        lv_label_set_text(opt_lbl_status, "Clearing Cache...");
    }
    if (opt_lbl_file) {
        lv_label_set_text(opt_lbl_file, "Scanning cache...");
    }
    if (opt_lbl_percent) {
        lv_label_set_text(opt_lbl_percent, "Preparing...");
    }
    if (opt_bar) {
        lv_bar_set_value(opt_bar, 0, LV_ANIM_OFF);
    }
    if (opt_btn_cancel) {
        lv_obj_add_flag(opt_btn_cancel, LV_OBJ_FLAG_HIDDEN);
    }

    lv_task_handler();
#endif
}

void LVGLManager::updateClearCacheProgress(size_t current, size_t total, const char* filename) {
#if !defined(NATIVE_TEST)
    if (opt_screen == nullptr) showClearCacheScreen();

    if (opt_lbl_status) {
        lv_label_set_text(opt_lbl_status, "Clearing Cache...");
    }

    if (opt_lbl_file && filename) {
        char fileStr[128];
        snprintf(fileStr, sizeof(fileStr), "Deleting:\n%s", filename);
        lv_label_set_text(opt_lbl_file, fileStr);
    }

    int percentage = (total == 0) ? 0 : (current * 100) / total;
    if (opt_bar) {
        lv_bar_set_value(opt_bar, percentage, LV_ANIM_OFF);
    }

    if (opt_lbl_percent) {
        char percentStr[64];
        if (total == 0) {
            snprintf(percentStr, sizeof(percentStr), "%s", filename);
        } else {
            snprintf(percentStr, sizeof(percentStr), "Cleared %zu/%zu (%d%%)", current, total, percentage);
        }
        lv_label_set_text(opt_lbl_percent, percentStr);
    }

    lv_task_handler();
#endif
}

void LVGLManager::hideClearCacheScreen() {
#if !defined(NATIVE_TEST)
    hideOptimizationScreen();
#endif
}

#if !defined(NATIVE_TEST)
static void ap_disable_btn_event_cb(lv_event_t * e) {
    isWifiEnabled = false;
    if (wifiManager != nullptr) {
        wifiManager->stop();
    }
    pendingExitSettings = true;
}
#endif

void LVGLManager::showAPModeScreen(const char* apSSID, const char* ipAddress) {
#if !defined(NATIVE_TEST)
    if (ap_screen != nullptr) return;

    ap_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ap_screen, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).base), 0);
    
    // Title label
    lv_obj_t * lbl_title = lv_label_create(ap_screen);
    lv_label_set_text(lbl_title, "CYD Photo Frame");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 15);

    // Status label
    lv_obj_t * lbl_status = lv_label_create(ap_screen);
    lv_label_set_text(lbl_status, "Captive Portal Active");
    lv_obj_set_style_text_color(lbl_status, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).mauve), 0);
    lv_obj_align(lbl_status, LV_ALIGN_TOP_MID, 0, 50);

    // Info details
    lv_obj_t * lbl_info = lv_label_create(ap_screen);
    char infoBuf[256];
    snprintf(infoBuf, sizeof(infoBuf), 
             "SSID: %s\nIP: %s\n\nConnect your device to setup WiFi", 
             apSSID, ipAddress);
    lv_label_set_text(lbl_info, infoBuf);
    lv_obj_set_style_text_align(lbl_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_info, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).text), 0);
    lv_obj_align(lbl_info, LV_ALIGN_TOP_MID, 0, 80);

    // Disable WiFi Button
    lv_obj_t * btn_disable = lv_btn_create(ap_screen);
    lv_obj_set_size(btn_disable, 200, 50);
    lv_obj_align(btn_disable, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(btn_disable, get_lv_color(getCatppuccinFlavor(currentThemeFlavor).red), 0);
    lv_obj_add_event_cb(btn_disable, ap_disable_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_btn = lv_label_create(btn_disable);
    lv_label_set_text(lbl_btn, "Disable WiFi");
    lv_obj_set_style_text_font(lbl_btn, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_btn);

    lv_scr_load(ap_screen);
    lv_task_handler();
#endif
}

void LVGLManager::hideAPModeScreen() {
#if !defined(NATIVE_TEST)
    if (ap_screen != nullptr) {
        lv_obj_t * old_scr = ap_screen;
        lv_obj_t * blank_scr = lv_obj_create(NULL);
        lv_obj_set_style_bg_opa(blank_scr, LV_OPA_TRANSP, 0);
        lv_scr_load(blank_scr);
        lv_obj_del(old_scr);
        ap_screen = nullptr;
    }
#endif
}
