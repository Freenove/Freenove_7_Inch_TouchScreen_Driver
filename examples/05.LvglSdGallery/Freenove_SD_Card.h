/**
 * @file Freenove_SD_Card.h
 * @brief On-board TF slot helper (SDMMC 4-bit) for Freenove ESP32-P4 panel kits.
 *
 * Power sequence and pins match the Freenove SD card hardware:
 *   CLK=43 CMD=44 D0=39 D1=40 D2=41 D3=42 POWER=45 LDO=VO4
 *
 * Mount point defaults to "/sdcard", which must match LV_FS_POSIX_PATH in
 * lvgl/src/lv_conf.h so LVGL can open files as "S:/...".
 *
 * Usage:
 *   Freenove_SD_Card sd;
 *   sd.begin();
 */

#pragma once

#include <stdint.h>

#ifndef FREENOVE_SD_MOUNT_POINT
#define FREENOVE_SD_MOUNT_POINT  "/sdcard"
#endif

class Freenove_SD_Card {
public:
    /**
     * @brief Power-cycle the card, configure SDMMC pins, and mount the filesystem.
     *
     * @param[in] mountPoint  VFS mount path (default FREENOVE_SD_MOUNT_POINT)
     * @return true on success
     */
    bool begin(const char *mountPoint = FREENOVE_SD_MOUNT_POINT);

    /** @brief Whether begin() succeeded */
    bool isReady() const { return _ready; }

    /** @brief Card size in bytes (0 if not mounted) */
    uint64_t cardSize() const;

    /** @brief Current mount point, or nullptr if not mounted */
    const char *mountPoint() const { return _ready ? _mountPoint : nullptr; }

private:
    bool _ready = false;
    const char *_mountPoint = nullptr;

    void powerOn();
};
