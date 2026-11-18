/*
 * SPDX-FileCopyrightText: 2026 Freenove
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file Freenove_7_Inch_Panel.h
 * @brief Arduino driver for the Freenove 7-inch 1024x600 MIPI-DSI C-type panel
 *        (EK79007 LCD + GT911 touch + on-panel control MCU) on ESP32-P4.
 *
 * Architecture of this panel:
 *   - LCD:     EK79007, MIPI-DSI 2 data lanes @ 1000Mbps, DPI 52MHz, RGB565, 1024x600
 *   - MCU:     on-panel MCU at I2C address 0x45, register 0x95 = power/reset control,
 *              register 0x96 = backlight brightness (0x00..0xFF)
 *   - Touch:   GT911 at I2C address 0x5D (fallback 0x14), sharing the same Wire bus
 *
 * The whole initialization order is handled by begin():
 *   Wire -> panel MCU power-on -> DSI PHY LDO -> MIPI-DSI bus -> EK79007 init
 *   -> framebuffer -> GT911 probe
 *
 * Typical usage:
 * @code{cpp}
 *   Freenove_7_Inch_Panel panel;
 *   panel.begin();                 // I2C 7/8, everything ready
 *   panel.setBacklight(70);
 *   uint16_t *fb = (uint16_t *)panel.getFrameBuffer();
 *   fb[0] = 0xF800;                // draw directly, or hand it to LVGL
 *   uint16_t x, y;
 *   if (panel.getTouch(&x, &y)) { ... }
 * @endcode
 */

#pragma once

#include <Arduino.h>

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "Freenove_7_Inch_TouchScreen_Driver only supports ESP32-P4"
#endif

#include <stdint.h>
#include <stddef.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A single touch point read from GT911
 */
typedef struct {
    uint8_t trackId;    /*!< Track id (finger identifier) */
    uint16_t x;         /*!< X coordinate in panel pixels */
    uint16_t y;         /*!< Y coordinate in panel pixels */
    uint16_t size;      /*!< Touch pressure/size (chip dependent) */
} Freenove_TouchPoint;

#ifdef __cplusplus
}
#endif

/**
 * @brief Driver class of the Freenove 7-inch C-type panel
 *
 * Inherits Arduino `Print`: after `setCursor()` / `setTextColor()` / `setTextScale()`,
 * `panel.print("hello")` / `panel.println()` draw text directly on the screen.
 */
class Freenove_7_Inch_Panel : public Print {
public:
    static constexpr uint16_t WIDTH   = 1024;               /*!< Panel width in pixels */
    static constexpr uint16_t HEIGHT  = 600;                /*!< Panel height in pixels */
    static constexpr size_t   FB_SIZE = WIDTH * HEIGHT * 2; /*!< Framebuffer size in bytes (RGB565) */

    /**
     * @brief Initialize the whole display pipeline.
     *
     * @param[in] sda      I2C SDA pin (default 7)
     * @param[in] scl      I2C SCL pin (default 8)
     * @param[in] i2cFreq  I2C clock frequency (default 400kHz)
     * @return true on success, false on failure (see the serial log for details)
     *
     * @note Must be called once before any other member function.
     *       `Wire` is initialized here; do not call `Wire.begin()` with other pins.
     */
    bool begin(int8_t sda = 7, int8_t scl = 8, uint32_t i2cFreq = 400000UL);

    /**
     * @brief Release all resources (panel, DSI bus, PHY LDO, Wire bus)
     */
    void end();

    /**
     * @brief Whether begin() succeeded
     */
    bool isReady() const { return _ready; }

    /**
     * @brief Set the backlight brightness through the on-panel MCU
     *
     * @param[in] percent Brightness 0..100 (clamped)
     */
    void setBacklight(uint8_t percent);

    /**
     * @brief The current backlight brightness percentage (0..100)
     */
    uint8_t getBacklight() const { return _backlight; }

    /**
     * @brief Get the RGB565 framebuffer pointer (1024x600, one single buffer).
     *
     * The MIPI-DPI hardware refreshes the panel from this buffer continuously.
     * Draw into it directly, or pass it to LVGL as draw buffer.
     *
     * @return Framebuffer pointer, nullptr if not ready
     */
    void *getFrameBuffer() const { return _fb; }

    /**
     * @brief Get the framebuffer size in bytes
     */
    size_t getFrameBufferSize() const { return FB_SIZE; }

    /**
     * @brief Write back the whole framebuffer from CPU cache to PSRAM.
     *
     * The DPI hardware fetches the framebuffer from PSRAM directly, so every
     * direct CPU write to the framebuffer MUST be followed by a cache sync,
     * otherwise the hardware reads stale data (typically the last-written
     * part of the frame appears as garbage on the bottom of the screen).
     *
     * For LVGL: call this in the flush callback after the LVGL draw buffer
     * has been copied into the framebuffer.
     */
    void syncFrameBuffer();

    /**
     * @brief Write back only a range of framebuffer lines from CPU cache to PSRAM
     *
     * Same as syncFrameBuffer() but limited to lines [yStart, yEnd) - much
     * cheaper for partial updates (e.g. one LVGL area per flush).
     *
     * @param[in] yStart First line to sync (inclusive)
     * @param[in] yEnd   Line after the last one to sync (exclusive)
     */
    void syncFrameBufferLines(uint16_t yStart, uint16_t yEnd);

    /**
     * @brief Read the first touch point (convenience wrapper)
     *
     * @param[out] x  X coordinate
     * @param[out] y  Y coordinate
     * @return true if a finger is down, false otherwise
     */
    bool getTouch(uint16_t *x, uint16_t *y);

    /**
     * @brief Read all current touch points from GT911
     *
     * @param[out] points    Array to fill
     * @param[in]  maxPoints Capacity of the array (up to 5)
     * @return Number of points actually read (0 if no touch)
     */
    uint8_t getTouchPoints(Freenove_TouchPoint *points, uint8_t maxPoints);

    //////////////////////////////////////////////////// Drawing ////////////////////////////////////////////////////

    /**
     * @brief Convert RGB888 to RGB565
     */
    static uint16_t color565(uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Enable/disable automatic cache sync after each drawing call
     *
     * Disabled by default: drawing functions only write into the framebuffer,
     * and the user must call syncFrameBuffer() to show the result. When enabled,
     * every drawing call also syncs the lines it touched (convenient but slower
     * for batch drawing).
     */
    void setAutoSync(bool enabled) { _autoSync = enabled; }

    /**
     * @brief Whether automatic cache sync is enabled
     */
    bool getAutoSync() const { return _autoSync; }

    /**
     * @brief Draw a single pixel (no clipping check overhead, coordinates are clamped)
     */
    void drawPixel(uint16_t x, uint16_t y, uint16_t color);

    /**
     * @brief Fill a rectangle
     */
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

    /**
     * @brief Fill the whole screen
     */
    void fillScreen(uint16_t color);

    /**
     * @brief Clear the whole screen to black (shortcut of fillScreen(0))
     */
    void clear() { fillScreen(0x0000); }

    /**
     * @brief Draw a line (Bresenham)
     */
    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

    /**
     * @brief Draw a single ASCII character with the built-in 8x16 font
     *
     * @param[in] c         Character 0x20..0x7E (others are rendered as space)
     * @param[in] fg        Foreground color
     * @param[in] bg        Background color
     * @param[in] scale     Integer scale factor (>= 1), glyph cell becomes 8*scale x 16*scale
     * @param[in] bgOpaque  false = transparent background (only foreground pixels drawn)
     */
    void drawChar(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg,
                  uint8_t scale = 1, bool bgOpaque = true);

    /**
     * @brief Draw a string with the built-in 8x16 font ('\n' starts a new line)
     */
    void drawString(const char *str, uint16_t x, uint16_t y, uint16_t fg, uint16_t bg,
                    uint8_t scale = 1, bool bgOpaque = true);

    /**
     * @brief Draw a raw RGB565 image (array in flash or RAM)
     *
     * @param[in] x, y   Top-left position on the panel
     * @param[in] w, h   Image size in pixels
     * @param[in] data   RGB565 pixel array, row-major, w*h entries
     */
    void drawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data);

    ////////////////////////////////////////////////// Display configuration //////////////////////////////////////////////////
    
    /**
     * @brief Turn the panel display on (DCS 0x29, the DSI link stays up)
     */
    void displayOn();
    
    /**
     * @brief Turn the panel display off (DCS 0x28, low power mode)
     */
    void displayOff();
    
    /**
     * @brief Whether the panel display is currently on
     */
    bool isDisplayOn() const { return _displayOn; }
    
    /**
     * @brief Mirror the picture horizontally (written through the panel MADCTL
     *        command, takes effect immediately)
     */
    void setDisplayMirrorX(bool on);
    
    /**
     * @brief Mirror the picture vertically (panel MADCTL command)
     */
    void setDisplayMirrorY(bool on);
    
    /**
     * @brief Invert the picture colors (panel INVON/INVOFF command)
     */
    void setInvertColor(bool on);
    
    /**
     * @brief Mirror the reported touch X coordinate (software transform, applied
     *        to the values returned by getTouch()/getTouchPoints())
     */
    void setTouchMirrorX(bool on) { _touchMirrorX = on; }
    
    /**
     * @brief Mirror the reported touch Y coordinate (software transform)
     */
    void setTouchMirrorY(bool on) { _touchMirrorY = on; }
    
    /**
     * @brief Swap the reported touch X/Y coordinates (software transform).
     *        Combination of swap + mirrors corrects any touch orientation.
     */
    void setTouchSwapXY(bool on) { _touchSwapXY = on; }
    
    /**
     * @brief Backlight full brightness (shortcut of setBacklight(100))
     */
    void backlightOn() { setBacklight(100); }
    
    /**
     * @brief Backlight off (shortcut of setBacklight(0))
     */
    void backlightOff() { setBacklight(0); }
    
    ////////////////////////////////////////////////// Text cursor (Print) //////////////////////////////////////////////////

    /**
     * @brief Set the text cursor position (top-left of the next printed character)
     */
    void setCursor(uint16_t x, uint16_t y);

    /**
     * @brief Set text colors used by print()/println()
     *
     * @param[in] fg        Foreground color
     * @param[in] bg        Background color
     * @param[in] bgOpaque  false = transparent background
     */
    void setTextColor(uint16_t fg, uint16_t bg, bool bgOpaque = true);

    /**
     * @brief Set the text scale factor used by print()/println() (>= 1)
     */
    void setTextScale(uint8_t scale);

    /**
     * @brief Print interface implementation: draws the character at the text
     *        cursor and advances it ('\n' starts a new line)
     */
    size_t write(uint8_t c) override;

private:
    bool _ready = false;
    uint8_t _backlight = 0;
    void *_fb = nullptr;
    esp_lcd_dsi_bus_handle_t _dsiBus = nullptr;
    esp_lcd_panel_io_handle_t _io = nullptr;
    esp_lcd_panel_handle_t _panel = nullptr;
    esp_ldo_channel_handle_t _phyLdo = nullptr;

    // display state
    bool _displayOn = true;
    bool _displayMirrorX = false;
    bool _displayMirrorY = false;
    bool _invertColor = false;

    // touch transform state
    bool _touchMirrorX = false;
    bool _touchMirrorY = false;
    bool _touchSwapXY = false;

    // drawing state
    bool _autoSync = false;
    int16_t _cursorX = 0;
    int16_t _cursorY = 0;
    uint16_t _textFg = 0xFFFF;
    uint16_t _textBg = 0x0000;
    bool _textBgOpaque = true;
    uint8_t _textScale = 1;

    // internal helpers (no argument validation beyond _ready/_fb)
    void writePixel(uint16_t x, uint16_t y, uint16_t color);
    void drawCharInternal(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg,
                          uint8_t scale, bool bgOpaque);
    void syncLinesIfAuto(uint16_t yStart, uint16_t yEnd);
};
