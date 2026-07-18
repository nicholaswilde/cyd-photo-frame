#include "lvgl_manager.h"
#include <stddef.h>
#include <stdlib.h>

#if !defined(NATIVE_TEST)
#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "touch_manager.h"
#include "slideshow_timer.h"
#include "app_state.h"

extern TFT_eSPI tft;

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
            pixelX = touchX;
            pixelY = touchY;
        } else {
            pixelX = (long)(touchX - 200) * disp_w / (3800 - 200);
            pixelY = (long)(touchY - 200) * disp_h / (3800 - 200);
            if (pixelX < 0) pixelX = 0;
            if (pixelX > disp_w) pixelX = disp_w;
            if (pixelY < 0) pixelY = 0;
            if (pixelY > disp_h) pixelY = disp_h;
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

static void brightness_slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    currentBrightness = (int)lv_slider_get_value(slider);
#if defined(TFT_BL) && (TFT_BL >= 0)
    if (!isAutoBrightness) {
        analogWrite(TFT_BL, currentBrightness);
    }
#endif
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

static void exit_button_event_cb(lv_event_t * e) {
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
    lv_obj_set_style_bg_color(settings_screen, lv_color_make(30, 30, 46), 0);
    
    lv_obj_t * title = lv_label_create(settings_screen);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, lv_color_make(205, 214, 244), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    lv_obj_t * list = lv_obj_create(settings_screen);
    lv_obj_set_size(list, LV_PCT(95), LV_PCT(70));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(list, lv_color_make(30, 30, 46), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 5, 0);
    lv_obj_set_style_pad_row(list, 10, 0);
    
    // 1. Brightness Slider
    lv_obj_t * row_bright = lv_obj_create(list);
    lv_obj_set_size(row_bright, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_bright, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_bright, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_bright, lv_color_make(49, 50, 68), 0);
    lv_obj_set_style_border_width(row_bright, 0, 0);
    lv_obj_set_style_pad_all(row_bright, 5, 0);

    lv_obj_t * lbl_bright = lv_label_create(row_bright);
    lv_label_set_text(lbl_bright, "Brightness");
    lv_obj_set_style_text_color(lbl_bright, lv_color_make(205, 214, 244), 0);

    lv_obj_t * slider_bright = lv_slider_create(row_bright);
    slider_bright_ptr = slider_bright;
    lv_obj_set_size(slider_bright, 80, 10);
    lv_obj_set_style_pad_right(row_bright, 15, 0);
    lv_slider_set_range(slider_bright, 25, 255);
    lv_slider_set_value(slider_bright, currentBrightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_bright, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // 2. Auto Brightness
    lv_obj_t * row_auto = lv_obj_create(list);
    lv_obj_set_size(row_auto, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_auto, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_auto, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_auto, lv_color_make(49, 50, 68), 0);
    lv_obj_set_style_border_width(row_auto, 0, 0);
    lv_obj_set_style_pad_all(row_auto, 5, 0);

    lv_obj_t * lbl_auto = lv_label_create(row_auto);
    lv_label_set_text(lbl_auto, "Auto Brightness (LDR)");
    lv_obj_set_style_text_color(lbl_auto, lv_color_make(205, 214, 244), 0);

    lv_obj_t * sw_auto = lv_switch_create(row_auto);
    if (isAutoBrightness) {
        lv_obj_add_state(sw_auto, LV_STATE_CHECKED);
        lv_obj_add_state(slider_bright, LV_STATE_DISABLED);
    }
    lv_obj_add_event_cb(sw_auto, auto_brightness_switch_event_cb, LV_EVENT_VALUE_CHANGED, slider_bright);
    
    // 3. Delay Dropdown
    lv_obj_t * row_delay = lv_obj_create(list);
    lv_obj_set_size(row_delay, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_delay, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_delay, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_delay, lv_color_make(49, 50, 68), 0);
    lv_obj_set_style_border_width(row_delay, 0, 0);
    lv_obj_set_style_pad_all(row_delay, 5, 0);

    lv_obj_t * lbl_delay = lv_label_create(row_delay);
    lv_label_set_text(lbl_delay, "Delay");
    lv_obj_set_style_text_color(lbl_delay, lv_color_make(205, 214, 244), 0);

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
    lv_obj_set_size(row_random, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_random, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_random, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_random, lv_color_make(49, 50, 68), 0);
    lv_obj_set_style_border_width(row_random, 0, 0);
    lv_obj_set_style_pad_all(row_random, 5, 0);

    lv_obj_t * lbl_random = lv_label_create(row_random);
    lv_label_set_text(lbl_random, "Random Mode");
    lv_obj_set_style_text_color(lbl_random, lv_color_make(205, 214, 244), 0);

    lv_obj_t * sw_random = lv_switch_create(row_random);
    if (isRandomMode) lv_obj_add_state(sw_random, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_random, random_mode_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // 5. Show Filename
    lv_obj_t * row_filename = lv_obj_create(list);
    lv_obj_set_size(row_filename, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_filename, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_filename, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_filename, lv_color_make(49, 50, 68), 0);
    lv_obj_set_style_border_width(row_filename, 0, 0);
    lv_obj_set_style_pad_all(row_filename, 5, 0);

    lv_obj_t * lbl_filename = lv_label_create(row_filename);
    lv_label_set_text(lbl_filename, "Show Filename");
    lv_obj_set_style_text_color(lbl_filename, lv_color_make(205, 214, 244), 0);

    lv_obj_t * sw_filename = lv_switch_create(row_filename);
    if (showFilename) lv_obj_add_state(sw_filename, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_filename, show_filename_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // 6. Inactivity Sleep
    lv_obj_t * row_sleep = lv_obj_create(list);
    lv_obj_set_size(row_sleep, LV_PCT(100), 40);
    lv_obj_set_flex_flow(row_sleep, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_sleep, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row_sleep, lv_color_make(49, 50, 68), 0);
    lv_obj_set_style_border_width(row_sleep, 0, 0);
    lv_obj_set_style_pad_all(row_sleep, 5, 0);

    lv_obj_t * lbl_sleep = lv_label_create(row_sleep);
    lv_label_set_text(lbl_sleep, "Inactivity Sleep");
    lv_obj_set_style_text_color(lbl_sleep, lv_color_make(205, 214, 244), 0);

    lv_obj_t * sw_sleep = lv_switch_create(row_sleep);
    if (isInactivitySleep) lv_obj_add_state(sw_sleep, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_sleep, inactivity_sleep_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 7. Save & Exit Button
    lv_obj_t * btn_exit = lv_btn_create(settings_screen);
    lv_obj_set_size(btn_exit, 100, 30);
    lv_obj_align(btn_exit, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(btn_exit, lv_color_make(166, 227, 161), 0);
    
    lv_obj_t * lbl_exit = lv_label_create(btn_exit);
    lv_label_set_text(lbl_exit, "Save & Exit");
    lv_obj_set_style_text_color(lbl_exit, lv_color_make(17, 17, 27), 0);
    lv_obj_align(lbl_exit, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_add_event_cb(btn_exit, exit_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_scr_load(settings_screen);
#endif
}

void LVGLManager::hideSettings() {
#if !defined(NATIVE_TEST)
    if (settings_screen != nullptr) {
        lv_obj_t * old_scr = settings_screen;
        lv_scr_load(lv_obj_create(NULL));
        lv_obj_del(old_scr);
        settings_screen = nullptr;
        slider_bright_ptr = nullptr;
    }
#endif
}
