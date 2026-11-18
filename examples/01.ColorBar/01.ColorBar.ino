/**
 * @file 01.ColorBar.ino
 * @brief Initialize the panel, draw RGB color bars, print touch coordinates.
 *
 * Board: ESP32-P4 (enable PSRAM in the Arduino IDE board options)
 */

#include <Freenove_7_Inch_Panel.h>

Freenove_7_Inch_Panel panel;

void setup()
{
    Serial.begin(115200);
    Serial.println("01.ColorBar: init panel...");

    if (!panel.begin()) {
        Serial.println("Panel init failed, halt");
        while (true) {
            delay(1000);
        }
    }

    panel.setBacklight(70);

    const uint16_t colors[3] = {0x001F, 0x07E0, 0xF800};  // B, G, R
    const uint16_t barH = Freenove_7_Inch_Panel::HEIGHT / 3;
    panel.fillRect(0, 0, Freenove_7_Inch_Panel::WIDTH, barH, colors[0]);
    panel.fillRect(0, barH, Freenove_7_Inch_Panel::WIDTH, barH, colors[1]);
    panel.fillRect(0, 2 * barH, Freenove_7_Inch_Panel::WIDTH,
                   Freenove_7_Inch_Panel::HEIGHT - 2 * barH, colors[2]);
    panel.syncFrameBuffer();

    Serial.println("01.ColorBar: ready");
}

void loop()
{
    uint16_t x, y;
    if (panel.getTouch(&x, &y)) {
        Serial.printf("Touch: x=%u y=%u\n", x, y);
    }
    delay(10);
}
