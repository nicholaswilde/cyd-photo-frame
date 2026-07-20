#include "lvgl_manager.h"
#include <stddef.h>
#include <stdlib.h>
#include "catppuccin.h"

extern int currentThemeFlavor;
extern int currentOrientation;

#if !defined(NATIVE_TEST)
#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
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

static void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    if (currentState != STATE_SETTINGS) {
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
        bool isCapacitive = (touchX <= disp_w * 2 && touchY <= disp_h * 2);
        if (isCapacitive) {
            int w_land = disp_w > disp_h ? disp_w : disp_h;
            int h_land = disp_w < disp_h ? disp_w : disp_h;
            
            bool rawIsPortrait = (touchX <= h_land && touchY <= w_land);
            
            if (rawIsPortrait) {
                switch (tft.getRotation()) {
                    case 0: // Portrait Rev / Native Portrait
                        pixelX = h_land - touchX;
                        pixelY = touchY;
                        break;
                    case 1: // Landscape
                        pixelX = touchY;
                        pixelY = h_land - touchX;
                        break;
                    case 2: // Portrait
                        pixelX = touchX;
                        pixelY = w_land - touchY;
                        break;
                    case 3: // Landscape Rev
                        pixelX = w_land - touchY;
                        pixelY = touchX;
                        break;
                    default:
                        pixelX = touchY;
                        pixelY = touchX;
                        break;
                }
            } else {
                switch (tft.getRotation()) {
                    case 0: // Portrait
                        pixelX = touchY;
                        pixelY = w_land - touchX;
                        break;
                    case 1: // Landscape
                        pixelX = touchX;
                        pixelY = touchY;
                        break;
                    case 2: // Portrait Rev
                        pixelX = h_land - touchY;
                        pixelY = touchX;
                        break;
                    case 3: // Landscape Rev
                        pixelX = w_land - touchX;
                        pixelY = h_land - touchY;
                        break;
                    default:
                        pixelX = touchX;
                        pixelY = touchY;
                        break;
                }
            }
            
            if (pixelX < 0) pixelX = 0;
            if (pixelX >= disp_w) pixelX = disp_w - 1;
            if (pixelY < 0) pixelY = 0;
            if (pixelY >= disp_h) pixelY = disp_h - 1;
        } else {
            int w_land = disp_w > disp_h ? disp_w : disp_h;
            int h_land = disp_w < disp_h ? disp_w : disp_h;
            int lx = (long)(touchX - 200) * w_land / (3800 - 200);
            int ly = (long)(touchY - 200) * h_land / (3800 - 200);
            if (lx < 0) lx = 0;
            if (lx > w_land) lx = w_land;
            if (ly < 0) ly = 0;
            if (ly > h_land) ly = h_land;
            
            switch (tft.getRotation()) {
                case 0: // Portrait Rev
                    pixelX = h_land - ly;
                    pixelY = lx;
                    break;
                case 1: // Landscape
                    pixelX = lx;
                    pixelY = ly;
                    break;
                case 2: // Portrait
                    pixelX = ly;
                    pixelY = w_land - lx;
                    break;
                case 3: // Landscape Rev
                    pixelX = w_land - lx;
                    pixelY = h_land - ly;
                    break;
                default:
                    pixelX = lx;
                    pixelY = ly;
                    break;
            }
        }
        data->state = LV_INDEV_STATE_PR;
        data->point.x = pixelX;
        data->point.y = pixelY;
    }
}

static void (*exitCallback)() = nullptr;
static lv_obj_t* settings_screen = nullptr;
static lv_obj_t* slider_bright_ptr = nullptr;

extern int currentBrightness;
extern bool showFilename;
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
    currentThemeFlavor = (int)lv_dropdown_get_selected(dropdown) + 1;
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
        if (settings_screen && lv_scr_act() == settings_screen && isAutoBrightness) {
            if (slider_bright_ptr) {
                lv_slider_set_value(slider_bright_ptr, currentBrightness, LV_ANIM_OFF);
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
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    lv_obj_t * list = lv_obj_create(settings_screen);
    lv_obj_set_size(list, LV_PCT(95), LV_PCT(74));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 35);
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
    lv_dropdown_set_selected(dd_theme, currentThemeFlavor - 1);
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
    }
#endif
}
