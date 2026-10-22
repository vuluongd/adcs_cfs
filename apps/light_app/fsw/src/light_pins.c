#include "light_sensor.h"
#include <stddef.h>

static const LIGHT_PinConfig_t Pi3 = {
    .board_name = "Raspberry Pi 3, 40-pin header",
    .sda_gpio = 2, .scl_gpio = 3,
    .sda_physical = 3, .scl_physical = 5,
    .power_physical = 1, .ground_physical = 6,
    .left_address = 0x23, .right_address = 0x5c,
    .linux_device = "/dev/i2c-1"
};
static const LIGHT_PinConfig_t Esp32 = {
    .board_name = "Generic classic ESP32; verify your board labels",
    .sda_gpio = 21, .scl_gpio = 22,
    .sda_physical = -1, .scl_physical = -1,
    .power_physical = -1, .ground_physical = -1,
    .left_address = 0x23, .right_address = 0x5c,
    .linux_device = NULL
};

const LIGHT_PinConfig_t *LIGHT_GetPinConfig(LIGHT_Board_t board)
{
    switch (board)
    {
        case LIGHT_BOARD_RPI3: return &Pi3;
        case LIGHT_BOARD_ESP32_CLASSIC: return &Esp32;
        default: return NULL;
    }
}
