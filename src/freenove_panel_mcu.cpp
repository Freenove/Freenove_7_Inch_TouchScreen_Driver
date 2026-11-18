/*
 * SPDX-FileCopyrightText: 2026 Freenove
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freenove_panel_mcu.h"
#include <Wire.h>

bool freenovePanelMcuWriteReg(uint8_t reg, uint8_t val)
{
    Wire.beginTransmission(FREENOVE_PANEL_MCU_I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);

    return (Wire.endTransmission() == 0);
}

bool freenovePanelMcuPowerOn(void)
{
    static const uint8_t ctrl_seq[] = {0x11, 0x17};
    for (uint8_t val : ctrl_seq) {
        if (!freenovePanelMcuWriteReg(FREENOVE_PANEL_MCU_CTRL_REG, val)) {
            return false;
        }
    }

    // Backlight off then on explicitly
    if (!freenovePanelMcuWriteReg(FREENOVE_PANEL_MCU_BL_REG, 0x00)) {
        return false;
    }

    return freenovePanelMcuWriteReg(FREENOVE_PANEL_MCU_BL_REG, 0xFF);
}

bool freenovePanelMcuSetBacklight(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    const uint8_t val = (uint8_t)((255 * percent) / 100);

    return freenovePanelMcuWriteReg(FREENOVE_PANEL_MCU_BL_REG, val);
}
