/**
 * @file Freenove_LVGL_Port.h
 * @brief LVGL v9 display + touch port for Freenove_7_Inch_TouchScreen_Driver examples.
 *
 * Usage:
 *   Freenove_7_Inch_Panel panel;
 *   Freenove_LVGL_Port lvglPort(panel);
 *   panel.begin();
 *   lvglPort.begin();   // applies DISPLAY/TOUCH_DIRECTION, registers drivers
 *   // loop: lvglPort.handler();
 *
 * Orientation macros (edit below):
 *   DISPLAY_DIRECTION  — screen mirrors via setDisplayMirrorX/Y (0..3)
 *   TOUCH_DIRECTION    — touch mirrors / swap (0..7)
 */

#pragma once

#include <lvgl.h>
#include <Freenove_7_Inch_Panel.h>

/**
 * Display direction (panel MADCTL mirrors):
 *   0 = normal
 *   1 = mirror X
 *   2 = mirror Y
 *   3 = mirror X + Y
 */
#ifndef DISPLAY_DIRECTION
#define DISPLAY_DIRECTION  (0)
#endif

/**
 * Touch direction (software transform on GT911 coords):
 *   0 = normal
 *   1 = mirror X
 *   2 = mirror Y
 *   3 = mirror X + Y
 *   4 = swap XY
 *   5 = swap XY + mirror X
 *   6 = swap XY + mirror Y
 *   7 = swap XY + mirror X + Y
 */
#ifndef TOUCH_DIRECTION
#define TOUCH_DIRECTION  (3)
#endif

class Freenove_LVGL_Port {
public:
    static constexpr uint16_t DEFAULT_BUF_LINES = 40;
    static constexpr uint32_t DEFAULT_TOUCH_HOLD_MS = 50;

    explicit Freenove_LVGL_Port(Freenove_7_Inch_Panel &panel);

    /**
     * @brief Apply DISPLAY_DIRECTION / TOUCH_DIRECTION, then init LVGL
     *        display flush + pointer indev.
     *
     * Calls lv_init(), lv_tick_set_cb(millis), and (if LV_USE_LOG) registers a
     * Serial log print callback. Safe to call only after panel.begin() has succeeded.
     *
     * @param[in] bufLines  Partial render buffer height in lines (default 40)
     * @param[in] touchHoldMs  Keep last touch point this long when GT911 has
     *                         no new frame yet (avoids press/release chatter)
     * @return true on success
     */
    bool begin(uint16_t bufLines = DEFAULT_BUF_LINES,
               uint32_t touchHoldMs = DEFAULT_TOUCH_HOLD_MS);

    /** @brief Apply DISPLAY_DIRECTION to the panel */
    void applyDisplayDirection(uint8_t dir = DISPLAY_DIRECTION);

    /** @brief Apply TOUCH_DIRECTION to the panel */
    void applyTouchDirection(uint8_t dir = TOUCH_DIRECTION);

    /** @brief Run LVGL timers (call from loop()) */
    void handler() { lv_timer_handler(); }

    /** @brief Optional calibration cursor updated on each touch sample */
    void setTouchCursor(lv_obj_t *cursor) { _cursor = cursor; }

    lv_display_t *display() const { return _disp; }
    lv_indev_t *indev() const { return _indev; }
    bool isReady() const { return _ready; }

private:
    Freenove_7_Inch_Panel &_panel;
    lv_display_t *_disp = nullptr;
    lv_indev_t *_indev = nullptr;
    uint16_t *_buf = nullptr;
    size_t _bufBytes = 0;
    lv_obj_t *_cursor = nullptr;
    bool _ready = false;

    // Touch hold state (GT911 buffer-ready is slower than LVGL polling)
    uint32_t _touchHoldMs = DEFAULT_TOUCH_HOLD_MS;
    bool _pressed = false;
    uint16_t _lastX = 0;
    uint16_t _lastY = 0;
    uint32_t _lastMs = 0;

    static void flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap);
    static void touchCb(lv_indev_t *indev, lv_indev_data_t *data);
#if LV_USE_LOG != 0
    static void logPrintCb(lv_log_level_t level, const char *buf);
#endif

    void onFlush(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap);
    void onTouch(lv_indev_data_t *data);
};
