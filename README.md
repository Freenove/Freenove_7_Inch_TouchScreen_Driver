# Freenove_7_Inch_TouchScreen_Driver

Arduino library for the Freenove **7-inch 1024×600** MIPI-DSI C-type touch panel on **ESP32-P4** only.

- **LCD**: EK79007, MIPI-DSI 2-lane, RGB565, 1024×600
- **Touch**: GT911 over `Wire` (I2C `0x5D`, fallback `0x14`)
- **Panel MCU**: I2C `0x45` — power / reset / backlight
- **Framebuffer**: RGB565 in PSRAM for direct draw or LVGL

Other ESP32 chips fail at compile time (`CONFIG_IDF_TARGET_ESP32P4` check).

## Install

Copy this folder into the Arduino `libraries` directory, or use **Sketch → Include Library → Add .ZIP Library…**.

Requires **arduino-esp32** with board **ESP32P4**, **PSRAM Enabled**, Flash **16MB**.

### LVGL (`lv_conf.h`)

For LVGL examples (`04`–`06`), also install sibling library **lvgl** (v9.x).  
This library ships a Freenove-tuned config at `src/lv_conf.h`. Copy it to **one** of these places so LVGL can pick it up:

1. **Arduino libraries root** — e.g. `Documents/Arduino/libraries/lv_conf.h`  
2. **Inside LVGL** — e.g. `Documents/Arduino/libraries/lvgl/src/lv_conf.h`

Do not leave two different copies that disagree. Prefer keeping Freenove’s file as the source of truth and re-copy after you change it.

See `docs/LVGL_Configuration_Guide.md` for what the macros mean.

## Quick start (Freenove ESP32-P4 defaults)

Default I2C: **SDA=7**, **SCL=8**.

```cpp
#include <Freenove_7_Inch_Panel.h>

Freenove_7_Inch_Panel panel;

void setup() {
  Serial.begin(115200);
  if (!panel.begin()) {
    Serial.println("panel begin failed");
    while (true) delay(1000);
  }
  panel.setBacklight(70);

  panel.fillScreen(panel.color565(0, 0, 255));
  panel.syncFrameBuffer();   // required after CPU writes to the framebuffer
}

void loop() {
  uint16_t x, y;
  if (panel.getTouch(&x, &y)) {
    Serial.printf("touch %u,%u\n", x, y);
  }
}
```

`begin()` initializes `Wire` — do not call `Wire.begin()` with other pins afterward.

Direct framebuffer writes (and most drawing APIs with auto-sync off) need `syncFrameBuffer()` / `syncFrameBufferLines()` so DPI reads fresh data from PSRAM.

## API overview

| Group | Functions |
|-------|-----------|
| Life cycle | `begin`, `end`, `isReady` |
| Backlight / display | `setBacklight`, `getBacklight`, `backlightOn/Off`, `displayOn/Off`, `setInvertColor` |
| Framebuffer | `getFrameBuffer`, `getFrameBufferSize`, `syncFrameBuffer`, `syncFrameBufferLines`, `setAutoSync` |
| Touch | `getTouch`, `getTouchPoints`, `setTouchMirrorX/Y`, `setTouchSwapXY` |
| Orientation | `setDisplayMirrorX/Y` |
| Drawing | `color565`, `drawPixel`, `fillRect`, `fillScreen`, `clear`, `drawLine`, `drawChar`, `drawString`, `drawImage` |
| Text (`Print`) | `setCursor`, `setTextColor`, `setTextScale`, `print` / `println` |

Panel size: `Freenove_7_Inch_Panel::WIDTH` / `HEIGHT` (1024×600).

## Examples

Open via **File → Examples → Freenove_7_Inch_TouchScreen_Driver → …**.  
Board: **ESP32P4**, **PSRAM Enabled**, Serial **115200**.

Suggested order: `01` → `03` (hardware) → `02` (draw API) → `04` → `06` (LVGL) → `05` (SD + decode).

### 01.ColorBar — bring-up

First sketch to verify the panel powers on.

- Calls `panel.begin()` + `setBacklight(70)`
- Draws three full-width bars (blue / green / red) with `fillRect`, then `syncFrameBuffer()`
- In `loop()`, prints touch coordinates to Serial

**Expect:** clear RGB bands; touch prints `x,y` on Serial. If black / `begin` fails, check power, flat cable, I2C, and PSRAM.

### 02.Draw — drawing API

Demo of software drawing into the RGB565 framebuffer (no LVGL).

- Background fill, `drawString` / `drawChar`, Arduino `Print` (`print` / `println`)
- Color swatches, lines, and a generated 48×48 `drawImage` bitmap
- One `syncFrameBuffer()` after the batch (auto-sync stays off)

**Expect:** title text, color boxes, crossed lines, small gradient image. Touch still dumps to Serial.

### 03.Touch — GT911 alignment

Calibrate / check touch vs picture.

- Guide frame: border, corner + center crosses
- Finger down → red dots on screen + multi-point dump on Serial (up to 5)
- If axes are wrong, uncomment `setTouchMirrorX/Y` / `setTouchSwapXY` in the sketch

**Expect:** red dots under your finger at corners and center. Fix orientation here before LVGL demos.

### 04.LvglBasic — minimal LVGL

Smallest LVGL v9 UI on this panel.

- Companion: `Freenove_LVGL_Port.*` (flush + touch indev; edit `DISPLAY_DIRECTION` / `TOUCH_DIRECTION`)
- One label + one centered button; press/release logs to Serial
- Requires sibling library **lvgl** v9.x

**Expect:** “Hello…” label and a tappable “Touch me” button. Use this as the template for your own UI.

### 05.LvglSdGallery — SD images

LVGL gallery reading files from the on-board TF slot.

- Companion: `Freenove_LVGL_Port.*` + `Freenove_SD_Card.*` (mount `/sdcard`)
- Put on SD card **root** (FAT): `1.jpg`, `2.bmp`, `3.png` (demo uses ~600×600, centered)
- LVGL paths `S:/…` map to `/sdcard/…` via `lv_conf.h` (`LV_USE_FS_POSIX`, letter `S`)
- Left / right buttons cycle images (wraps)

**Expect:** centered image + prev/next. Needs `LODEPNG` / `TJPGD` / `BMP` and CLIB malloc + PSRAM in `lv_conf.h`. Decode fails are usually memory/path, not “decoder off”.

### 06.LvglArduino — official widgets demo

Port of LVGL’s Arduino sketch with the full **widgets** demo.

- Companion: `Freenove_LVGL_Port.*` + sketch `build_opt.h` (`-DFREENOVE_LVGL_DEMO_WIDGETS=1`)
- Demo sources are `#include`’d from upstream `lvgl/demos/` inside this library (`.c` bundle) — **do not** copy demos into `lvgl/src/`
- `FREENOVE_UI_MODE`: `1` = `lv_demo_widgets()` (default), `0` = Hello label only
- Needs `LV_USE_DEMO_WIDGETS 1` and related fonts / FLEX / GRID in `lv_conf.h`

**Expect:** Profile / Analytics / Shop tabs and interactive widgets. Details: `docs/LVGL_Configuration_Guide.md`.

## Notes

- MIPI-DSI path is **ESP32-P4 only**.
- For LVGL, keep `Freenove_LVGL_Port.*` next to your `.ino` (same as examples `04`–`06`).
- More LVGL / `lv_conf.h` detail: `docs/LVGL_Configuration_Guide.md`.
