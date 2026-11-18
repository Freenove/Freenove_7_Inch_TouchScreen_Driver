#include "Freenove_SD_Card.h"
#include <Arduino.h>
#include <SD_MMC.h>

static const uint8_t SD_CLK_PIN   = 43;
static const uint8_t SD_CMD_PIN   = 44;
static const uint8_t SD_D0_PIN    = 39;
static const uint8_t SD_D1_PIN    = 40;
static const uint8_t SD_D2_PIN    = 41;
static const uint8_t SD_D3_PIN    = 42;
static const uint8_t SD_POWER_PIN = 45;  // Q5 P-channel: HIGH=off, LOW=on
static const int     SD_LDO_CHAN  = 4;   // card IO / pull-ups from LDO VO4

void Freenove_SD_Card::powerOn()
{
    pinMode(SD_POWER_PIN, OUTPUT);
    digitalWrite(SD_POWER_PIN, HIGH);  // off
    delay(100);
    digitalWrite(SD_POWER_PIN, LOW);   // on
    delay(50);
}

bool Freenove_SD_Card::begin(const char *mountPoint)
{
    if (_ready) {
        return true;
    }
    if (mountPoint == nullptr || mountPoint[0] == '\0') {
        mountPoint = FREENOVE_SD_MOUNT_POINT;
    }

    powerOn();
    SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN, SD_D1_PIN, SD_D2_PIN, SD_D3_PIN);
    SD_MMC.setPowerChannel(SD_LDO_CHAN);

    // 4-bit mode, never auto-format, 40 MHz bus
    if (!SD_MMC.begin(mountPoint, false, false, 40000)) {
        _mountPoint = nullptr;
        _ready = false;
        return false;
    }

    _mountPoint = mountPoint;
    _ready = true;
    return true;
}

uint64_t Freenove_SD_Card::cardSize() const
{
    return _ready ? SD_MMC.cardSize() : 0;
}
