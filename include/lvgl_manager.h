#ifndef LVGL_MANAGER_H
#define LVGL_MANAGER_H

#include <stddef.h>

class LVGLManager {
public:
    static void init(int width, int height);
    static void handle();
    
    static bool isInitialized();
    static int getWidth();
    static int getHeight();
    
    static void setExitCallback(void (*exit_cb)());
    static void showSettings();
    static void hideSettings();

    static void showSDError();
    static void showOptimizationScreen();
    static void updateCalculationProgress(size_t current, size_t total, const char* filename, size_t needsOptCount);
    static void updateOptimizationProgress(size_t current, size_t total, const char* filename);
    static void setOptimizationCancelling();
    static void hideOptimizationScreen();
    static void setCancelCallback(void (*cancel_cb)());
    static void setClearCacheCallback(void (*clear_cache_cb)());
    static void showClearCacheScreen();
    static void updateClearCacheProgress(size_t current, size_t total, const char* filename);
    static void hideClearCacheScreen();
};

#endif // LVGL_MANAGER_H
