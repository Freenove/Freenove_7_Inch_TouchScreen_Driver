/**
 * @file 05.LvglSdGallery.ino
 * @brief Show 600x600 images from the SD card root with prev/next buttons.
 *
 * Put these files on the SD card root (FAT formatted):
 *   1.jpg
 *   2.bmp
 *   3.png
 *
 * The image is centered on the 1024x600 panel. Left button = previous,
 * right button = next (wraps around).
 *
 * Companion files:
 *   Freenove_LVGL_Port.*  — display flush + touch indev class
 *                           (edit DISPLAY_DIRECTION / TOUCH_DIRECTION there)
 *   Freenove_SD_Card.*    — on-board TF slot mount helper
 *
 * Requires: LVGL library v9.x (Arduino Library Manager)
 * Board: ESP32-P4 (enable PSRAM in the Arduino IDE board options)
 */

#include <lvgl.h>
#include <Freenove_7_Inch_Panel.h>
#include "Freenove_LVGL_Port.h"
#include "Freenove_SD_Card.h"

Freenove_7_Inch_Panel panel;
Freenove_LVGL_Port lvglPort(panel);
Freenove_SD_Card sd;

static const uint16_t IMAGE_W = 600;
static const uint16_t IMAGE_H = 600;
static const uint16_t BTN_W   = 120;
static const uint16_t BTN_H   = 80;

// LVGL FS letter 'S' maps to /sdcard (see lv_conf.h)
static const char *s_imagePaths[] = {
    "S:/1.jpg",
    "S:/2.bmp",
    "S:/3.png",
};
static const uint8_t s_imageCount =
    (uint8_t)(sizeof(s_imagePaths) / sizeof(s_imagePaths[0]));

static lv_obj_t *s_img = nullptr;
static lv_obj_t *s_title = nullptr;
static uint8_t s_index = 0;

static void showImage(uint8_t index)
{
    if (s_img == nullptr || index >= s_imageCount) {
        return;
    }
    s_index = index;
    const char *path = s_imagePaths[s_index];
    lv_image_set_src(s_img, path);
    if (s_title != nullptr) {
        lv_label_set_text_fmt(s_title, "%u / %u  %s",
                              (unsigned)(s_index + 1),
                              (unsigned)s_imageCount,
                              path);
    }
    Serial.printf("Show %s\n", path);
}

static void onPrev(lv_event_t *e)
{
    LV_UNUSED(e);
    uint8_t next = (s_index == 0) ? (uint8_t)(s_imageCount - 1) : (uint8_t)(s_index - 1);
    showImage(next);
}

static void onNext(lv_event_t *e)
{
    LV_UNUSED(e);
    uint8_t next = (uint8_t)((s_index + 1) % s_imageCount);
    showImage(next);
}

static lv_obj_t *createSideButton(const char *text, lv_align_t align, int32_t xOfs,
                                  lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn, BTN_W, BTN_H);
    lv_obj_align(btn, align, xOfs, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}

static void createUi()
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0);

    s_title = lv_label_create(scr);
    lv_obj_set_style_text_color(s_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 8);

    // Centered 600x600 image on 1024x600
    s_img = lv_image_create(scr);
    lv_obj_set_size(s_img, IMAGE_W, IMAGE_H);
    lv_obj_align(s_img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_img, lv_color_hex(0x404040), 0);
    lv_obj_set_style_bg_opa(s_img, LV_OPA_COVER, 0);

    const int32_t sideGap = (Freenove_7_Inch_Panel::WIDTH - IMAGE_W) / 4 - (BTN_W / 2);
    createSideButton(LV_SYMBOL_LEFT " Prev", LV_ALIGN_LEFT_MID, sideGap, onPrev);
    createSideButton("Next " LV_SYMBOL_RIGHT, LV_ALIGN_RIGHT_MID, -sideGap, onNext);

    showImage(0);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("05.LvglSdGallery: init panel...");

    if (!panel.begin()) {
        Serial.println("Panel init failed, halt");
        while (true) {
            delay(1000);
        }
    }
    panel.setBacklight(70);

    if (!sd.begin()) {
        Serial.println("SD mount failed, halt");
        while (true) {
            delay(1000);
        }
    }
    Serial.printf("SD mounted at %s, %llu MB\n",
                  sd.mountPoint(),
                  sd.cardSize() / (1024ULL * 1024ULL));

    if (!lvglPort.begin()) {
        Serial.println("LVGL port init failed, halt");
        while (true) {
            delay(1000);
        }
    }
    Serial.printf("DISPLAY_DIRECTION=%u TOUCH_DIRECTION=%u\n",
                  (unsigned)DISPLAY_DIRECTION, (unsigned)TOUCH_DIRECTION);

    createUi();
    Serial.println("05.LvglSdGallery: ready");
}

void loop()
{
    lvglPort.handler();
    delay(5);
}
