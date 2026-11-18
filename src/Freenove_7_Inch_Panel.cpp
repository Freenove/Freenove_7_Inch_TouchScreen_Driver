/*
 * SPDX-FileCopyrightText: 2026 Freenove
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Freenove_7_Inch_Panel.h"
#include <Wire.h>
#include <string.h>
#include "esp_cache.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "ek79007/esp_lcd_ek79007.h"
#include "freenove_panel_mcu.h"
#include "freenove_panel_gt911.h"
#include "freenove_font_8x16.h"

/* MIPI DSI PHY power: on-chip LDO VO3 feeds VDD_MIPI_DPHY (same as esp-bsp) */
#define FREENOVE_PHY_LDO_CHAN         (3)
#define FREENOVE_PHY_LDO_VOLTAGE_MV   (2500)

/* MIPI DSI bus parameters of this panel */
#define FREENOVE_DSI_LANE_NUM         (2)
#define FREENOVE_DSI_LANE_RATE_MBPS   (1000)

bool Freenove_7_Inch_Panel::begin(int8_t sda, int8_t scl, uint32_t i2cFreq)
{
    if (_ready) {
        return true;
    }

    // Declare every config struct up front so the failure path can `goto cleanup`
    // without crossing a variable initialization.
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = FREENOVE_PHY_LDO_CHAN,
        .voltage_mv = FREENOVE_PHY_LDO_VOLTAGE_MV,
    };
    esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id = 0,
        .num_data_lanes = FREENOVE_DSI_LANE_NUM,
        .phy_clk_src = (mipi_dsi_phy_pllref_clock_source_t)0,   // 0 = let the driver pick its default clock source
        .lane_bit_rate_mbps = FREENOVE_DSI_LANE_RATE_MBPS,
    };
    esp_lcd_dbi_io_config_t dbi_cfg = EK79007_PANEL_IO_DBI_CONFIG();
    esp_lcd_dpi_panel_config_t dpi_cfg = EK79007_1024_600_PANEL_60HZ_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    ek79007_vendor_config_t vendor_cfg = {
        .init_cmds = nullptr,   // use the default vendor init sequence
        .init_cmds_size = 0,
        .mipi_config = {
            .dsi_bus = nullptr,   // filled in once the DSI bus exists
            .dpi_config = &dpi_cfg,
            .lane_num = FREENOVE_DSI_LANE_NUM,
        },
    };
    esp_lcd_panel_dev_config_t dev_cfg = {
        .reset_gpio_num = -1,   // reset is issued by the panel MCU (+ DCS SWRESET below)
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_cfg,
    };
    void *fb = nullptr;

    // 1. I2C bus (Arduino Wire, shared by the panel MCU and the GT911).
    //    begin() owns the bus; end() deliberately leaves it running so other
    //    devices on the same Wire are not torn down.
    Wire.begin(sda, scl, i2cFreq);

    // 2. Panel MCU power-on / reset sequence (must run before the panel init,
    //    otherwise the panel and the touch controller are not powered yet)
    if (!freenovePanelMcuPowerOn()) {
        log_e("Panel MCU power-on failed (addr 0x%02X)", FREENOVE_PANEL_MCU_I2C_ADDR);
        goto cleanup;
    }

    // 3. MIPI DSI PHY power (on-chip LDO VO3, 2.5V)
    if (esp_ldo_acquire_channel(&ldo_cfg, &_phyLdo) != ESP_OK) {
        log_e("Acquire LDO channel %d for DPHY failed", FREENOVE_PHY_LDO_CHAN);
        goto cleanup;
    }

    // 4. MIPI DSI bus
    if (esp_lcd_new_dsi_bus(&bus_cfg, &_dsiBus) != ESP_OK) {
        log_e("New DSI bus failed");
        goto cleanup;
    }
    vendor_cfg.mipi_config.dsi_bus = _dsiBus;

    // 5. DBI panel IO, used to send DCS commands to the EK79007
    if (esp_lcd_new_panel_io_dbi(_dsiBus, &dbi_cfg, &_io) != ESP_OK) {
        log_e("New panel IO failed");
        goto cleanup;
    }

    // 6. EK79007 panel: DPI 1024x600 @ 52MHz, RGB565, single framebuffer
    if (esp_lcd_new_panel_ek79007(_io, &dev_cfg, &_panel) != ESP_OK) {
        log_e("New EK79007 panel failed");
        goto cleanup;
    }
    if (esp_lcd_panel_reset(_panel) != ESP_OK) {
        log_e("Panel reset failed");
        goto cleanup;
    }
    if (esp_lcd_panel_init(_panel) != ESP_OK) {
        log_e("Panel init failed");
        goto cleanup;
    }

    // 7. Framebuffer of the DPI panel
    if (esp_lcd_dpi_panel_get_frame_buffer(_panel, 1, &fb) != ESP_OK) {
        log_e("Get frame buffer failed");
        goto cleanup;
    }
    _fb = fb;

    // 8. Touch controller on the same Wire bus
    if (!freenovePanelTouchBegin()) {
        log_e("GT911 touch not found");
        goto cleanup;
    }

    _ready = true;
    log_i("Freenove_7_Inch_Panel ready: %dx%d RGB565, framebuffer @%p", WIDTH, HEIGHT, _fb);

    return true;

cleanup:
    // Release whatever was acquired so a failed begin() leaks nothing and a
    // retry starts from a clean state. end() is null-safe per handle.
    end();
    return false;
}

void Freenove_7_Inch_Panel::end()
{
    // Release in reverse acquisition order. Each handle is null-checked, so this
    // is safe after a partially-failed begin() too (no _ready gate).
    if (_panel != nullptr) {
        esp_lcd_panel_del(_panel);
        _panel = nullptr;
    }
    if (_io != nullptr) {
        esp_lcd_panel_io_del(_io);
        _io = nullptr;
    }
    if (_dsiBus != nullptr) {
        esp_lcd_del_dsi_bus(_dsiBus);
        _dsiBus = nullptr;
    }
    if (_phyLdo != nullptr) {
        esp_ldo_release_channel(_phyLdo);
        _phyLdo = nullptr;
    }
    _fb = nullptr;
    _ready = false;
    // Wire is intentionally NOT ended here: it is a shared bus and other devices
    // (or a subsequent begin()) may still rely on it.
}

void Freenove_7_Inch_Panel::syncFrameBuffer()
{
    if (!_ready || _fb == nullptr) {
        return;
    }
    const esp_err_t err = esp_cache_msync(_fb, FB_SIZE, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    if (err != ESP_OK) {
        log_e("Framebuffer cache sync failed (%d)", err);
    }
}

void Freenove_7_Inch_Panel::syncFrameBufferLines(uint16_t yStart, uint16_t yEnd)
{
    if (!_ready || _fb == nullptr) {
        return;
    }
    if (yEnd > HEIGHT) {
        yEnd = HEIGHT;
    }
    if (yStart >= yEnd) {
        return;
    }
    const size_t offset = (size_t)yStart * WIDTH * 2;
    const size_t len = (size_t)(yEnd - yStart) * WIDTH * 2;
    const esp_err_t err = esp_cache_msync((uint8_t *)_fb + offset, len, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    if (err != ESP_OK) {
        log_e("Framebuffer line cache sync failed (%d)", err);
    }
}

void Freenove_7_Inch_Panel::displayOn()
{
    if (!_ready || _displayOn) {
        return;
    }
    if (esp_lcd_panel_io_tx_param(_io, 0x29, nullptr, 0) != ESP_OK) {   // DCS SET_DISPLAY_ON
        log_e("Display on failed");

        return;
    }
    delay(20);
    _displayOn = true;
}

void Freenove_7_Inch_Panel::displayOff()
{
    if (!_ready || !_displayOn) {
        return;
    }
    if (esp_lcd_panel_io_tx_param(_io, 0x28, nullptr, 0) != ESP_OK) {   // DCS SET_DISPLAY_OFF
        log_e("Display off failed");

        return;
    }
    delay(20);
    _displayOn = false;
}

void Freenove_7_Inch_Panel::setDisplayMirrorX(bool on)
{
    if (!_ready || _displayMirrorX == on) {
        return;
    }
    _displayMirrorX = on;
    if (esp_lcd_panel_mirror(_panel, _displayMirrorX, _displayMirrorY) != ESP_OK) {
        log_e("Set mirror failed");
    }
}

void Freenove_7_Inch_Panel::setDisplayMirrorY(bool on)
{
    if (!_ready || _displayMirrorY == on) {
        return;
    }
    _displayMirrorY = on;
    if (esp_lcd_panel_mirror(_panel, _displayMirrorX, _displayMirrorY) != ESP_OK) {
        log_e("Set mirror failed");
    }
}

void Freenove_7_Inch_Panel::setInvertColor(bool on)
{
    if (!_ready || _invertColor == on) {
        return;
    }
    _invertColor = on;
    if (esp_lcd_panel_invert_color(_panel, on) != ESP_OK) {
        log_e("Set invert color failed");
    }
}

void Freenove_7_Inch_Panel::setBacklight(uint8_t percent)
{
    if (!_ready) {
        return;
    }
    if (percent > 100) {
        percent = 100;
    }
    if (!freenovePanelMcuSetBacklight(percent)) {
        log_e("Set backlight failed");

        return;
    }
    _backlight = percent;
}

bool Freenove_7_Inch_Panel::getTouch(uint16_t *x, uint16_t *y)
{
    Freenove_TouchPoint point;
    if (getTouchPoints(&point, 1) > 0) {
        if (x != nullptr) {
            *x = point.x;
        }
        if (y != nullptr) {
            *y = point.y;
        }

        return true;
    }

    return false;
}

uint8_t Freenove_7_Inch_Panel::getTouchPoints(Freenove_TouchPoint *points, uint8_t maxPoints)
{
    if (!_ready) {
        return 0;
    }

    uint8_t count = freenovePanelTouchRead(points, maxPoints);

    // Apply the software touch transform (swap, then mirrors)
    if (count > 0 && (_touchSwapXY || _touchMirrorX || _touchMirrorY)) {
        for (uint8_t i = 0; i < count; i++) {
            uint16_t x = points[i].x;
            uint16_t y = points[i].y;
            if (_touchSwapXY) {
                uint16_t t = x;
                x = y;
                y = t;
            }
            if (_touchMirrorX) {
                // Saturating mirror: a raw value beyond the axis range must not
                // underflow uint16 (that produced coords like 65286).
                x = (x >= WIDTH) ? 0 : (uint16_t)(WIDTH - 1 - x);
            }
            if (_touchMirrorY) {
                y = (y >= HEIGHT) ? 0 : (uint16_t)(HEIGHT - 1 - y);
            }
            points[i].x = x;
            points[i].y = y;
        }
    }

    return count;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Drawing /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint16_t Freenove_7_Inch_Panel::color565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void Freenove_7_Inch_Panel::writePixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (_fb == nullptr || x >= WIDTH || y >= HEIGHT) {
        return;
    }
    ((uint16_t *)_fb)[(size_t)y * WIDTH + x] = color;
}

void Freenove_7_Inch_Panel::syncLinesIfAuto(uint16_t yStart, uint16_t yEnd)
{
    if (_autoSync && _ready) {
        syncFrameBufferLines(yStart, yEnd);
    }
}

void Freenove_7_Inch_Panel::drawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (!_ready) {
        return;
    }
    writePixel(x, y, color);
    if (_autoSync) {
        syncFrameBufferLines(y, y + 1);
    }
}

void Freenove_7_Inch_Panel::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (!_ready || _fb == nullptr || w == 0 || h == 0) {
        return;
    }
    uint16_t xs = x;
    uint16_t ys = y;
    uint16_t xe = (uint32_t)x + w > WIDTH ? WIDTH : (uint16_t)(x + w);
    uint16_t ye = (uint32_t)y + h > HEIGHT ? HEIGHT : (uint16_t)(y + h);
    if (xs >= WIDTH || ys >= HEIGHT) {
        return;
    }

    for (uint16_t row = ys; row < ye; row++) {
        uint16_t *dst = (uint16_t *)_fb + (size_t)row * WIDTH + xs;
        for (uint16_t col = xs; col < xe; col++) {
            *dst++ = color;
        }
    }

    if (_autoSync) {
        syncFrameBufferLines(ys, ye);
    }
}

void Freenove_7_Inch_Panel::fillScreen(uint16_t color)
{
    fillRect(0, 0, WIDTH, HEIGHT, color);
}

void Freenove_7_Inch_Panel::drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    if (!_ready) {
        return;
    }

    int32_t dx = abs((int32_t)x1 - (int32_t)x0);
    int32_t dy = -abs((int32_t)y1 - (int32_t)y0);
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy;
    uint16_t minY = y0 < y1 ? y0 : y1;
    uint16_t maxY = y0 > y1 ? y0 : y1;

    while (true) {
        writePixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }

    if (_autoSync) {
        syncFrameBufferLines(minY, maxY + 1);
    }
}

void Freenove_7_Inch_Panel::drawCharInternal(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg,
                                             uint8_t scale, bool bgOpaque)
{
    uint8_t idx = (c >= FREENOVE_FONT_FIRST && c <= FREENOVE_FONT_LAST) ? (uint8_t)(c - FREENOVE_FONT_FIRST) : 0;

    for (uint8_t row = 0; row < FREENOVE_FONT_H; row++) {
        const uint8_t bits = freenove_font_8x16[idx][row];
        for (uint8_t col = 0; col < FREENOVE_FONT_W; col++) {
            const bool on = bits & (0x80 >> col);
            if (!on && !bgOpaque) {
                continue;
            }
            const uint16_t color = on ? fg : bg;
            if (scale == 1) {
                writePixel(x + col, y + row, color);
            } else {
                for (uint8_t sy = 0; sy < scale; sy++) {
                    for (uint8_t sx = 0; sx < scale; sx++) {
                        writePixel(x + col * scale + sx, y + row * scale + sy, color);
                    }
                }
            }
        }
    }
}

void Freenove_7_Inch_Panel::drawChar(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg,
                                     uint8_t scale, bool bgOpaque)
{
    if (!_ready) {
        return;
    }
    if (scale == 0) {
        scale = 1;
    }
    drawCharInternal(x, y, c, fg, bg, scale, bgOpaque);
    if (_autoSync) {
        syncFrameBufferLines(y, (uint32_t)y + FREENOVE_FONT_H * scale > HEIGHT ? HEIGHT : y + FREENOVE_FONT_H * scale);
    }
}

void Freenove_7_Inch_Panel::drawString(const char *str, uint16_t x, uint16_t y, uint16_t fg, uint16_t bg,
                                       uint8_t scale, bool bgOpaque)
{
    if (!_ready || str == nullptr) {
        return;
    }
    if (scale == 0) {
        scale = 1;
    }

    uint16_t cx = x;
    uint16_t cy = y;
    uint16_t yStart = y;
    for (const char *p = str; *p != '\0'; p++) {
        if (*p == '\n') {
            cx = x;
            cy += FREENOVE_FONT_H * scale;
            if ((uint32_t)cy + FREENOVE_FONT_H * scale > HEIGHT) {
                break;
            }
            continue;
        }
        drawCharInternal(cx, cy, *p, fg, bg, scale, bgOpaque);
        cx += FREENOVE_FONT_W * scale;
        if ((uint32_t)cx + FREENOVE_FONT_W * scale > WIDTH) {
            cx = x;
            cy += FREENOVE_FONT_H * scale;
            if ((uint32_t)cy + FREENOVE_FONT_H * scale > HEIGHT) {
                break;
            }
        }
    }

    if (_autoSync) {
        uint16_t yEnd = (uint32_t)cy + FREENOVE_FONT_H * scale > HEIGHT ? HEIGHT : cy + FREENOVE_FONT_H * scale;
        syncFrameBufferLines(yStart, yEnd);
    }
}

void Freenove_7_Inch_Panel::drawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data)
{
    if (!_ready || _fb == nullptr || data == nullptr || w == 0 || h == 0) {
        return;
    }

    uint16_t ys = y;
    uint16_t ye = (uint32_t)y + h > HEIGHT ? HEIGHT : (uint16_t)(y + h);
    if (ys >= HEIGHT) {
        return;
    }
    uint16_t xs = x;
    uint16_t xe = (uint32_t)x + w > WIDTH ? WIDTH : (uint16_t)(x + w);
    if (xs >= WIDTH) {
        return;
    }

    const size_t count = xe - xs;
    for (uint16_t row = ys; row < ye; row++) {
        const uint16_t *src = data + (size_t)(row - y) * w + (xs - x);
        uint16_t *dst = (uint16_t *)_fb + (size_t)row * WIDTH + xs;
        memcpy(dst, src, count * sizeof(uint16_t));
    }

    if (_autoSync) {
        syncFrameBufferLines(ys, ye);
    }
}

////////////////////////////////////////////////// Text cursor (Print) ////////////////////////////////////////////////////

void Freenove_7_Inch_Panel::setCursor(uint16_t x, uint16_t y)
{
    _cursorX = x;
    _cursorY = y;
}

void Freenove_7_Inch_Panel::setTextColor(uint16_t fg, uint16_t bg, bool bgOpaque)
{
    _textFg = fg;
    _textBg = bg;
    _textBgOpaque = bgOpaque;
}

void Freenove_7_Inch_Panel::setTextScale(uint8_t scale)
{
    _textScale = scale == 0 ? 1 : scale;
}

size_t Freenove_7_Inch_Panel::write(uint8_t c)
{
    if (!_ready) {
        return 1;
    }
    if (c == '\r') {
        return 1;
    }
    if (c == '\n') {
        _cursorX = 0;
        _cursorY += FREENOVE_FONT_H * _textScale;
        if ((uint32_t)_cursorY + FREENOVE_FONT_H * _textScale > HEIGHT) {
            _cursorY = 0;
        }
        return 1;
    }

    drawCharInternal(_cursorX, _cursorY, (char)c, _textFg, _textBg, _textScale, _textBgOpaque);
    if (_autoSync) {
        uint16_t yEnd = (uint32_t)_cursorY + FREENOVE_FONT_H * _textScale > HEIGHT ? HEIGHT : _cursorY + FREENOVE_FONT_H * _textScale;
        syncFrameBufferLines(_cursorY, yEnd);
    }

    _cursorX += FREENOVE_FONT_W * _textScale;
    if ((uint32_t)_cursorX + FREENOVE_FONT_W * _textScale > WIDTH) {
        _cursorX = 0;
        _cursorY += FREENOVE_FONT_H * _textScale;
        if ((uint32_t)_cursorY + FREENOVE_FONT_H * _textScale > HEIGHT) {
            _cursorY = 0;
        }
    }

    return 1;
}
