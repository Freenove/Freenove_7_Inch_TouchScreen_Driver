/*
 * SPDX-FileCopyrightText: 2026 Freenove
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file freenove_panel_gt911.h
 * @brief Minimal GT911 touch reader over Arduino Wire.
 *
 * Only the polling path is implemented (no INT/RST pins, no config download):
 * the GT911 firmware is already configured for 1024x600, so coordinates are
 * returned as panel pixels with no software remapping.
 */

#pragma once

#include <stdint.h>
#include "Freenove_7_Inch_Panel.h"

#define FREENOVE_PANEL_TOUCH_MAX_POINTS  (5)

/**
 * @brief Probe the GT911 on the current Wire bus.
 *
 * Tries address 0x5D first, then 0x14. Reads and logs the product id.
 *
 * @return true if the GT911 answered
 */
bool freenovePanelTouchBegin(void);

/**
 * @brief Poll the current touch points.
 *
 * Must be called regularly (e.g. every 10-30ms). The status register is
 * cleared automatically after a successful read.
 *
 * @param[out] points    Array to fill (up to FREENOVE_PANEL_TOUCH_MAX_POINTS)
 * @param[in]  maxPoints Capacity of the array
 * @return Number of points actually read (0 if no touch or read error)
 */
uint8_t freenovePanelTouchRead(Freenove_TouchPoint *points, uint8_t maxPoints);
