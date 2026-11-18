#include "Freenove_LVGL_Port.h"
#include <string.h>
#include "esp_heap_caps.h"

Freenove_LVGL_Port::Freenove_LVGL_Port(Freenove_7_Inch_Panel &panel)
    : _panel(panel)
{
}

void Freenove_LVGL_Port::applyDisplayDirection(uint8_t dir)
{
    _panel.setDisplayMirrorX((dir & 0x01) != 0);
    _panel.setDisplayMirrorY((dir & 0x02) != 0);
}

void Freenove_LVGL_Port::applyTouchDirection(uint8_t dir)
{
    _panel.setTouchMirrorX((dir & 0x01) != 0);
    _panel.setTouchMirrorY((dir & 0x02) != 0);
    _panel.setTouchSwapXY((dir & 0x04) != 0);
}

bool Freenove_LVGL_Port::begin(uint16_t bufLines, uint32_t touchHoldMs)
{
    if (_ready) {
        return true;
    }
    if (!_panel.isReady()) {
        return false;
    }
    if (bufLines == 0) {
        bufLines = DEFAULT_BUF_LINES;
    }

    _touchHoldMs = touchHoldMs;

    applyDisplayDirection(DISPLAY_DIRECTION);
    applyTouchDirection(TOUCH_DIRECTION);

    lv_init();
    lv_tick_set_cb(millis);

#if LV_USE_LOG != 0
    lv_log_register_print_cb(logPrintCb);
#endif

    // Aligned draw buffer (LV_DRAW_BUF_ALIGN / ESP32-P4 cache line = 64)
    _bufBytes = (size_t)Freenove_7_Inch_Panel::WIDTH * bufLines * sizeof(uint16_t);
    _buf = (uint16_t *)heap_caps_aligned_alloc(64, _bufBytes, MALLOC_CAP_DEFAULT);
    if (_buf == nullptr) {
        return false;
    }

    _disp = lv_display_create(Freenove_7_Inch_Panel::WIDTH, Freenove_7_Inch_Panel::HEIGHT);
    lv_display_set_user_data(_disp, this);
    lv_display_set_flush_cb(_disp, flushCb);
    lv_display_set_buffers(_disp, _buf, nullptr, _bufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

    _indev = lv_indev_create();
    lv_indev_set_type(_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_user_data(_indev, this);
    lv_indev_set_read_cb(_indev, touchCb);

    // Keep the default screen fixed so out-of-bounds children do not scroll it.
    lv_obj_remove_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLLABLE);

    _ready = true;
    return true;
}

#if LV_USE_LOG != 0
void Freenove_LVGL_Port::logPrintCb(lv_log_level_t level, const char *buf)
{
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
}
#endif

void Freenove_LVGL_Port::flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap)
{
    Freenove_LVGL_Port *self = (Freenove_LVGL_Port *)lv_display_get_user_data(disp);
    if (self != nullptr) {
        self->onFlush(disp, area, pxMap);
    } else {
        lv_display_flush_ready(disp);
    }
}

void Freenove_LVGL_Port::onFlush(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap)
{
    uint16_t *fb = (uint16_t *)_panel.getFrameBuffer();
    if (fb == nullptr) {
        lv_display_flush_ready(disp);
        return;
    }

    const int32_t aw = area->x2 - area->x1 + 1;
    const int32_t x1 = area->x1 < 0 ? 0 : area->x1;
    const int32_t x2 = area->x2 >= (int32_t)Freenove_7_Inch_Panel::WIDTH
                           ? Freenove_7_Inch_Panel::WIDTH - 1
                           : area->x2;
    const int32_t y1 = area->y1 < 0 ? 0 : area->y1;
    const int32_t y2 = area->y2 >= (int32_t)Freenove_7_Inch_Panel::HEIGHT
                           ? Freenove_7_Inch_Panel::HEIGHT - 1
                           : area->y2;
    if (x1 > x2 || y1 > y2) {
        lv_display_flush_ready(disp);
        return;
    }

    const int32_t cw = x2 - x1 + 1;
    for (int32_t y = y1; y <= y2; y++) {
        const uint16_t *src = (const uint16_t *)pxMap
                            + (size_t)(y - area->y1) * aw + (x1 - area->x1);
        memcpy(fb + (size_t)y * Freenove_7_Inch_Panel::WIDTH + x1, src,
               cw * sizeof(uint16_t));
    }

    _panel.syncFrameBufferLines((uint16_t)y1, (uint16_t)(y2 + 1));
    lv_display_flush_ready(disp);
}

void Freenove_LVGL_Port::touchCb(lv_indev_t *indev, lv_indev_data_t *data)
{
    Freenove_LVGL_Port *self = (Freenove_LVGL_Port *)lv_indev_get_user_data(indev);
    if (self != nullptr) {
        self->onTouch(data);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void Freenove_LVGL_Port::onTouch(lv_indev_data_t *data)
{
    uint16_t x, y;
    if (_panel.getTouch(&x, &y)) {
        _pressed = true;
        _lastX = x;
        _lastY = y;
        _lastMs = millis();
        if (_cursor != nullptr) {
            lv_obj_set_pos(_cursor, (int32_t)x - 8, (int32_t)y - 8);
        }
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (_pressed && (millis() - _lastMs) < _touchHoldMs) {
        data->point.x = _lastX;
        data->point.y = _lastY;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        _pressed = false;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
