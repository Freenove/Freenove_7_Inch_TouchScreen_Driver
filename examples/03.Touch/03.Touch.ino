/**
 * @file 03.Touch.ino
 * @brief GT911 touch check: corner/center guides, Serial dump, on-screen dots.
 *
 * How to use:
 *   1. Flash to ESP32-P4 (enable PSRAM)
 *   2. Open Serial Monitor at 115200
 *   3. Touch the four corners and the center; the red dot should land under
 *      your finger. If axes are wrong, uncomment setTouchMirrorX/Y / SwapXY.
 *
 * Board: ESP32-P4 (enable PSRAM in the Arduino IDE board options)
 */

#include <Freenove_7_Inch_Panel.h>

Freenove_7_Inch_Panel panel;

static const uint8_t  TOUCH_MAX  = 5;
static const uint16_t COLOR_BG   = 0x1082;
static const uint16_t COLOR_GRID = 0xC618;
static const uint16_t COLOR_DOT  = 0xF800;
static const uint16_t COLOR_TEXT = 0xFFFF;

static void drawCross(uint16_t cx, uint16_t cy, uint16_t arm, uint16_t color)
{
    panel.drawLine(cx - arm, cy, cx + arm, cy, color);
    panel.drawLine(cx, cy - arm, cx, cy + arm, color);
}

static void drawDot(uint16_t x, uint16_t y, uint16_t r, uint16_t color)
{
    const uint16_t d = (uint16_t)(r * 2 + 1);
    const uint16_t x0 = (x > r) ? (uint16_t)(x - r) : 0;
    const uint16_t y0 = (y > r) ? (uint16_t)(y - r) : 0;
    panel.fillRect(x0, y0, d, d, color);
}

static void drawGuide()
{
    const uint16_t w = Freenove_7_Inch_Panel::WIDTH;
    const uint16_t h = Freenove_7_Inch_Panel::HEIGHT;

    panel.fillScreen(COLOR_BG);
    panel.drawLine(0, 0, w - 1, 0, COLOR_GRID);
    panel.drawLine(0, h - 1, w - 1, h - 1, COLOR_GRID);
    panel.drawLine(0, 0, 0, h - 1, COLOR_GRID);
    panel.drawLine(w - 1, 0, w - 1, h - 1, COLOR_GRID);

    const uint16_t arm = 24;
    drawCross(arm, arm, arm, COLOR_GRID);
    drawCross(w - 1 - arm, arm, arm, COLOR_GRID);
    drawCross(arm, h - 1 - arm, arm, COLOR_GRID);
    drawCross(w - 1 - arm, h - 1 - arm, arm, COLOR_GRID);
    drawCross(w / 2, h / 2, arm, COLOR_GRID);

    panel.drawString("03.Touch GT911", 16, 12, COLOR_TEXT, COLOR_BG, 2, true);
    panel.drawString("Touch corners & center; watch Serial + red dots",
                     16, 52, COLOR_GRID, COLOR_BG, 1, true);
    panel.syncFrameBuffer();
}

void setup()
{
    Serial.begin(115200);
    Serial.println("03.Touch: init panel...");

    if (!panel.begin()) {
        Serial.println("Panel init failed, halt");
        while (true) {
            delay(1000);
        }
    }

    panel.setBacklight(70);

    // Uncomment if touch axes do not match the picture:
    // panel.setTouchMirrorX(true);
    // panel.setTouchMirrorY(true);
    // panel.setTouchSwapXY(true);

    drawGuide();
    Serial.println("03.Touch: ready");
}

void loop()
{
    Freenove_TouchPoint points[TOUCH_MAX];
    const uint8_t count = panel.getTouchPoints(points, TOUCH_MAX);
    if (count == 0) {
        delay(10);
        return;
    }

    Serial.printf("touches=%u", count);
    uint16_t yMin = Freenove_7_Inch_Panel::HEIGHT;
    uint16_t yMax = 0;
    for (uint8_t i = 0; i < count; i++) {
        Serial.printf("  [%u] id=%u x=%u y=%u size=%u",
                      i, points[i].trackId, points[i].x, points[i].y, points[i].size);
        drawDot(points[i].x, points[i].y, 6, COLOR_DOT);

        const uint16_t y = points[i].y;
        const uint16_t y0 = (y > 6) ? (uint16_t)(y - 6) : 0;
        const uint16_t y1 = (uint16_t)min((uint32_t)y + 7, (uint32_t)Freenove_7_Inch_Panel::HEIGHT);
        if (y0 < yMin) {
            yMin = y0;
        }
        if (y1 > yMax) {
            yMax = y1;
        }
    }
    Serial.println();
    panel.syncFrameBufferLines(yMin, yMax);
    delay(10);
}
