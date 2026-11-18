/**
 * @file 04.LvglBasic.ino
 * @brief Minimal LVGL v9 example: one label and one touch button.
 *
 * Companion files:
 *   Freenove_LVGL_Port.*  — display flush + touch indev class
 *                           (edit DISPLAY_DIRECTION / TOUCH_DIRECTION there)
 *
 * Requires: LVGL library v9.x (Arduino Library Manager)
 * Board: ESP32-P4 (enable PSRAM in the Arduino IDE board options)
 */

#include <lvgl.h>
#include <Freenove_7_Inch_Panel.h>
#include "Freenove_LVGL_Port.h"

Freenove_7_Inch_Panel panel;
Freenove_LVGL_Port lvglPort(panel);

static lv_style_t s_btnPressedStyle;

static void btnEventCb(lv_event_t *e)
{
    switch (lv_event_get_code(e)) {
        case LV_EVENT_PRESSED:
            Serial.println("Button pressed");
            break;
        case LV_EVENT_RELEASED:
            Serial.println("Button released");
            break;
        default:
            break;
    }
}

static void createUi()
{
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello, LVGL on Freenove 7 Inch Panel!");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn, 160, 60);
    lv_obj_center(btn);

    lv_obj_t *btnLabel = lv_label_create(btn);
    lv_label_set_text(btnLabel, "Touch me");
    lv_obj_center(btnLabel);

    lv_style_init(&s_btnPressedStyle);
    lv_style_set_bg_color(&s_btnPressedStyle, lv_color_hex(0x1A5FB4));
    lv_obj_add_style(btn, &s_btnPressedStyle, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, btnEventCb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(btn, btnEventCb, LV_EVENT_RELEASED, nullptr);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("04.LvglBasic: init panel...");

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
    Serial.printf("DISPLAY_DIRECTION=%u TOUCH_DIRECTION=%u\n",
                  (unsigned)DISPLAY_DIRECTION, (unsigned)TOUCH_DIRECTION);

    createUi();
    Serial.println("04.LvglBasic: ready");
}

void loop()
{
    lvglPort.handler();
    delay(5);
}
