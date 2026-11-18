/**
 * @file 02.Draw.ino
 * @brief Demo of drawing APIs: fill, line, font, Print, and raw RGB565 image.
 *
 * Drawing writes the framebuffer only. Call panel.syncFrameBuffer() after a
 * batch, or enable panel.setAutoSync(true) to sync after every call.
 *
 * Board: ESP32-P4 (enable PSRAM in the Arduino IDE board options)
 */

#include <Freenove_7_Inch_Panel.h>

Freenove_7_Inch_Panel panel;

static const uint16_t IMG_W = 48;
static const uint16_t IMG_H = 48;
static uint16_t s_demoImage[IMG_W * IMG_H];

void setup()
{
    Serial.begin(115200);
    Serial.println("02.Draw: init panel...");

    if (!panel.begin()) {
        Serial.println("Panel init failed, halt");
        while (true) {
            delay(1000);
        }
    }

    panel.setBacklight(70);

    panel.fillScreen(panel.color565(16, 16, 32));

    panel.drawString("Freenove 7 Inch Panel", 16, 12, 0xFFFF, 0x0000, 2, false);
    panel.drawString("drawString() / drawChar() demo", 16, 50, 0x07E0, 0x0000, 1, false);

    panel.setCursor(16, 80);
    panel.setTextColor(0xFFE0, 0x0000);
    panel.setTextScale(1);
    panel.print("print() works like Serial, ");
    panel.println("cursor auto-advances!");

    const uint16_t swatch[6] = {
        panel.color565(255, 0, 0), panel.color565(0, 255, 0), panel.color565(0, 0, 255),
        panel.color565(255, 255, 0), panel.color565(0, 255, 255), panel.color565(255, 0, 255),
    };
    for (int i = 0; i < 6; i++) {
        panel.fillRect(16 + i * 40, 120, 32, 32, swatch[i]);
    }

    panel.drawLine(16, 170, 640, 200, 0xF81F);
    panel.drawLine(16, 200, 640, 170, 0x07FF);

    for (uint16_t y = 0; y < IMG_H; y++) {
        for (uint16_t x = 0; x < IMG_W; x++) {
            uint8_t r = (uint8_t)(x * 255 / IMG_W);
            uint8_t b = (uint8_t)(y * 255 / IMG_H);
            s_demoImage[y * IMG_W + x] = panel.color565(r, 255 - b, b);
        }
    }
    panel.drawImage(16, 230, IMG_W, IMG_H, s_demoImage);

    panel.syncFrameBuffer();
    Serial.println("02.Draw: ready");
}

void loop()
{
    uint16_t x, y;
    if (panel.getTouch(&x, &y)) {
        Serial.printf("Touch: x=%u y=%u\n", x, y);
    }
    delay(10);
}
