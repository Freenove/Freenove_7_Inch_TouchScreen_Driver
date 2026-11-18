/*
 * SPDX-FileCopyrightText: 2026 Freenove
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freenove_panel_gt911.h"
#include <Wire.h>

/* GT911 register map (only what the polling reader needs) */
#define GT911_REG_PRODUCT_ID    (0x8140)
#define GT911_REG_STATUS        (0x814E)
/* Point 1 starts at 0x814F (track id), then xL/xH/yL/yH/sizeL/sizeH/reserved.
 * Points 2..5 follow contiguously every 8 bytes (0x8157, 0x815F, ...). */
#define GT911_REG_POINTS        (0x814F)
#define GT911_POINT_SIZE        (8)

static uint8_t s_addr = 0;

static bool freenoveGt911WriteReg(uint16_t reg, const uint8_t *data, size_t len)
{
    Wire.beginTransmission(s_addr);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    for (size_t i = 0; i < len; i++) {
        Wire.write(data[i]);
    }

    return (Wire.endTransmission() == 0);
}

static bool freenoveGt911ReadRegs(uint16_t reg, uint8_t *buf, size_t len)
{
    // Set the register pointer with a repeated-start read
    Wire.beginTransmission(s_addr);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    if (Wire.requestFrom((uint8_t)s_addr, (uint8_t)len) != len) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        buf[i] = Wire.read();
    }

    return true;
}

bool freenovePanelTouchBegin(void)
{
    static const uint8_t addrs[] = {0x5D, 0x14};
    // The GT911 may still be booting right after the panel MCU powers it on,
    // so retry a few times before declaring it absent (a single NACK here used
    // to fail the whole panel begin()).
    for (uint8_t attempt = 0; attempt < 10; attempt++) {
        for (uint8_t addr : addrs) {
            uint8_t id[4] = {0};
            s_addr = addr;
            if (freenoveGt911ReadRegs(GT911_REG_PRODUCT_ID, id, sizeof(id))) {
                log_i("GT911 found at 0x%02X, product id: %c%c%c", addr, id[0], id[1], id[2]);

                return true;
            }
        }
        delay(10);
    }
    s_addr = 0;
    log_e("GT911 not found on the Wire bus");

    return false;
}

uint8_t freenovePanelTouchRead(Freenove_TouchPoint *points, uint8_t maxPoints)
{
    if (s_addr == 0 || points == nullptr || maxPoints == 0) {
        return 0;
    }

    uint8_t status = 0;
    if (!freenoveGt911ReadRegs(GT911_REG_STATUS, &status, 1)) {
        return 0;
    }
    if (!(status & 0x80)) {
        return 0;   // buffer not ready
    }

    uint8_t count = status & 0x0F;
    if (count > FREENOVE_PANEL_TOUCH_MAX_POINTS) {
        count = FREENOVE_PANEL_TOUCH_MAX_POINTS;
    }
    if (count > maxPoints) {
        count = maxPoints;
    }

    if (count > 0) {
        uint8_t buf[FREENOVE_PANEL_TOUCH_MAX_POINTS * GT911_POINT_SIZE];
        if (!freenoveGt911ReadRegs(GT911_REG_POINTS, buf, count * GT911_POINT_SIZE)) {
            count = 0;
        }
        for (uint8_t i = 0; i < count; i++) {
            const uint8_t *p = &buf[i * GT911_POINT_SIZE];
            // Same layout as GT911_Lite: id, xL, xH, yL, yH, sizeL, sizeH.
            points[i].trackId = p[0];
            uint16_t x = (uint16_t)(p[1] | (p[2] << 8));
            uint16_t y = (uint16_t)(p[3] | (p[4] << 8));
            if (x >= Freenove_7_Inch_Panel::WIDTH) {
                x = Freenove_7_Inch_Panel::WIDTH - 1;
            }
            if (y >= Freenove_7_Inch_Panel::HEIGHT) {
                y = Freenove_7_Inch_Panel::HEIGHT - 1;
            }
            points[i].x = x;
            points[i].y = y;
            points[i].size = (uint16_t)(p[5] | (p[6] << 8));
        }
    }

    // Clear the buffer-ready flag
    const uint8_t zero = 0;
    freenoveGt911WriteReg(GT911_REG_STATUS, &zero, 1);

    return count;
}
