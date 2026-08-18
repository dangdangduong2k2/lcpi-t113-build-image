/*
 * Minimal LVGL configuration for the LCPI-PC-T113 bring-up application.
 * DRM/KMS is preferred for page-flipped display; fbdev remains a safe fallback.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_OS                  LV_OS_PTHREAD
#define LV_USE_STDLIB_MALLOC       LV_STDLIB_CLIB
#define LV_COLOR_DEPTH             32
#define LV_DEF_REFR_PERIOD          17
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE (32 * 1024)
#define LV_DRAW_SW_DRAW_UNIT_CNT   2
#define LV_USE_DRAW_SW_ASM         LV_DRAW_SW_ASM_NEON

#define LV_USE_LOG                 0

#define LV_FONT_MONTSERRAT_14      1
#define LV_FONT_MONTSERRAT_20      1
#define LV_FONT_MONTSERRAT_24      1
#define LV_FONT_MONTSERRAT_26      1

#define LV_USE_FLEX                1
#define LV_USE_GRID                1

#define LV_USE_SYSMON              1
#define LV_SYSMON_REFR_PERIOD_DEF  1000
#define LV_USE_PERF_MONITOR        1
#define LV_USE_PERF_MONITOR_POS    LV_ALIGN_TOP_RIGHT
#define LV_USE_PERF_MONITOR_LOG_MODE 0

#define LV_USE_LINUX_FBDEV         1
#define LV_LINUX_FBDEV_BSD         0
#define LV_LINUX_FBDEV_RENDER_MODE LV_DISPLAY_RENDER_MODE_PARTIAL
#define LV_LINUX_FBDEV_BUFFER_COUNT 1
#define LV_LINUX_FBDEV_BUFFER_SIZE 40
#define LV_LINUX_FBDEV_MMAP        1

#define LV_USE_LINUX_DRM           1
#define LV_USE_LINUX_DRM_GBM_BUFFERS 0

#define LV_USE_EVDEV               1
#define LV_USE_GESTURE_RECOGNITION 0
#define LV_USE_TILEVIEW            1

#define LV_BUILD_DEMOS             1
#define LV_USE_DEMO_BENCHMARK      1
#define LV_DEMO_BENCHMARK_ALIGNED_FONTS 0
#define LV_USE_DEMO_WIDGETS        1

#endif
