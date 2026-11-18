/*
 * SPDX-FileCopyrightText: 2026 Freenove
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file freenove_panel_mcu.h
 * @brief Panel MCU control over Arduino Wire (power/reset + backlight).
 *
 * Register map of the on-panel MCU (C-type 7-inch panel):
 *   - I2C address   : 0x45 (7-bit)
 *   - Ctrl register : 0x95, power/reset control
 *   - BL register   : 0x96, backlight brightness (0x00..0xFF)
 */

#pragma once

#include <stdint.h>

#define FREENOVE_PANEL_MCU_I2C_ADDR     (0x45)
#define FREENOVE_PANEL_MCU_CTRL_REG     (0x95)
#define FREENOVE_PANEL_MCU_BL_REG       (0x96)

/**
 * @brief Write one register of the panel MCU.
 *
 * @param[in] reg Register address
 * @param[in] val Value to write
 * @return true on ACK, false on NACK/error
 */
bool freenovePanelMcuWriteReg(uint8_t reg, uint8_t val);

/**
 * @brief Send the built-in power-on / reset sequence:
 *        0x95 = 0x11, 0x95 = 0x17, 0x96 = 0x00, 0x96 = 0xFF.
 *
 * @note Must be called before initializing the MIPI-DSI panel, otherwise the
 *       panel (and the touch controller) are not powered yet.
 * @return true if the whole sequence was acknowledged
 */
bool freenovePanelMcuPowerOn(void);

/**
 * @brief Set the backlight brightness.
 *
 * @param[in] percent Brightness 0..100 (clamped), mapped to 0x00..0xFF
 * @return true on ACK
 */
bool freenovePanelMcuSetBacklight(uint8_t percent);
