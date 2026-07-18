#include "lvgl_manager.h"
#include <stddef.h>

#if !defined(NATIVE_TEST)
#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "touch_manager.h"

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
