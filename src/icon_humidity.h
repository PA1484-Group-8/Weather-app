#pragma once

#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Humidity icon (28x28, 1-bit alpha) */
extern const lv_img_dsc_t icon_humidity;

#ifdef __cplusplus
} /* extern "C" */
#endif
