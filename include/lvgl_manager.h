#ifndef LVGL_MANAGER_H
#define LVGL_MANAGER_H

class LVGLManager {
public:
    static void init(int width, int height);
    static void handle();
    
    static bool isInitialized();
    static int getWidth();
    static int getHeight();
};

#endif // LVGL_MANAGER_H
