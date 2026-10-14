/* XAC DINH CHAN: chon board -> tra GPIO, chan vat ly, dia chi I2C.
 * KHONG the tu nhan ra day SDA/SCL dang cam vao dau bang file nay.
 * Raspberry Pi 3: BCM GPIO2/SDA, GPIO3/SCL; physical 3/5.
 * ESP32 classic/Generic ESP32: GPIO21/SDA, GPIO22/SCL; board khac co the khac.
 * Sources: raspberrypi.com/documentation/computers/raspberry-pi.html
 *          docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html
 */
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
