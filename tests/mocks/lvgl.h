#ifndef MOCK_LVGL_H
#define MOCK_LVGL_H

#include <stdint.h>

typedef struct {
    void* user_data;
} lv_obj_t;

typedef struct {
    uint16_t limit;
} lv_disp_draw_buf_t;

struct _lv_disp_drv_t;

typedef struct _lv_disp_drv_t {
    uint16_t hor_res;
    uint16_t ver_res;
    void (*flush_cb)(struct _lv_disp_drv_t * disp_drv, const void * area, void * color_p);
    lv_disp_draw_buf_t * draw_buf;
} lv_disp_drv_t;

typedef enum {
    LV_INDEV_TYPE_POINTER
} lv_indev_type_t;

typedef struct {
    bool touched;
    struct {
        int16_t x;
        int16_t y;
    } point;
} lv_indev_data_t;

struct _lv_indev_drv_t;

typedef struct _lv_indev_drv_t {
    lv_indev_type_t type;
    void (*read_cb)(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
} lv_indev_drv_t;

inline void lv_init() {}
inline void lv_disp_draw_buf_init(lv_disp_draw_buf_t * buf, void * buf1, void * buf2, uint32_t size_in_px_cnt) {}
inline void lv_disp_drv_init(lv_disp_drv_t * driver) {
    driver->hor_res = 320;
    driver->ver_res = 240;
    driver->flush_cb = nullptr;
    driver->draw_buf = nullptr;
}
inline void lv_disp_drv_register(lv_disp_drv_t * driver) {}
inline void lv_indev_drv_init(lv_indev_drv_t * driver) {
    driver->type = LV_INDEV_TYPE_POINTER;
    driver->read_cb = nullptr;
}
inline void lv_indev_drv_register(lv_indev_drv_t * driver) {}
inline uint32_t lv_timer_handler() { return 0; }
inline lv_obj_t* lv_scr_act() { return nullptr; }

#define LV_SYMBOL_WIFI "[WiFi]"

#endif // MOCK_LVGL_H
