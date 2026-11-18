/**
 * @file 06.LvglArduino.ino
 * @brief Freenove port of the official LVGL Arduino sketch.
 *
 * Upstream reference:
 *   lvgl/examples/arduino/LVGL_Arduino/LVGL_Arduino.ino
 *
 * Companion files:
 *   Freenove_LVGL_Port.*  — display flush + touch + optional LVGL log
 *   build_opt.h           — -DFREENOVE_LVGL_DEMO_WIDGETS=1 so widgets demo
 *                           sources are compiled from Freenove library
 *                           (includes upstream lvgl/demos, does not copy into lvgl/src)
 *
 * UI selection: set FREENOVE_UI_MODE below.
 *
 * Requires: LVGL v9.x, LV_USE_DEMO_WIDGETS 1 in lv_conf.h (for widgets mode)
 * Board: ESP32-P4 (enable PSRAM)
 */

#include <lvgl.h>
#include <Freenove_7_Inch_Panel.h>
#include "Freenove_LVGL_Port.h"
#include <freenove_lvgl_demo_widgets.h>

Freenove_7_Inch_Panel panel;
Freenove_LVGL_Port lvglPort(panel);

/**
 * 0 = Hello label (official default)
 * 1 = lv_demo_widgets()
 */
#ifndef FREENOVE_UI_MODE
#define FREENOVE_UI_MODE 1
#endif

static void createUi()
{
#if FREENOVE_UI_MODE == 1
#if LV_USE_DEMO_WIDGETS
    lv_demo_widgets();
#else
#warning "FREENOVE_UI_MODE=1 needs LV_USE_DEMO_WIDGETS 1 in lv_conf.h"
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Enable LV_USE_DEMO_WIDGETS in lv_conf.h");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
#endif
#else
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello Arduino, I'm LVGL!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
#endif
}

void setup()
{
    String msg = "Hello Arduino! ";
    msg += String('V') + lv_version_major() + "." + lv_version_minor() + "." +
           lv_version_patch();

    Serial.begin(115200);
    Serial.println(msg);
    Serial.println("06.LvglArduino: init panel...");

    if (!panel.begin()) {
        Serial.println("Panel init failed, halt");
        while (true) {
            delay(1000);
        }
    }
    panel.setBacklight(70);

    if (!lvglPort.begin()) {
        Serial.println("LVGL port init failed, halt");
        while (true) {
            delay(1000);
        }
    }
    Serial.printf("DISPLAY_DIRECTION=%u TOUCH_DIRECTION=%u UI_MODE=%d\n",
                  (unsigned)DISPLAY_DIRECTION, (unsigned)TOUCH_DIRECTION,
                  (int)FREENOVE_UI_MODE);

    createUi();
    Serial.println("Setup done");
}

void loop()
{
    lvglPort.handler();
    delay(5);
}
