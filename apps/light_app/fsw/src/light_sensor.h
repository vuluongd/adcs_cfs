#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include "light_tracking.h"

typedef struct
{
    int fd_left;
    int fd_right;
    int hardware;
    uint64_t ready_after_ms;
    unsigned int sim_step;
} LIGHT_Sensor_t;

/* Linux: LIGHT_APP_BACKEND=sim (default) or i2c. Device defaults /dev/i2c-1. */
void LIGHT_SensorInit(LIGHT_Sensor_t *sensor);
void LIGHT_SensorClose(LIGHT_Sensor_t *sensor);
uint8_t LIGHT_SensorRead(LIGHT_Sensor_t *sensor, float *left_lux, float *right_lux);

/* light_pins.c: explicit board profile, NOT automatic physical-wire detection. */
typedef enum { LIGHT_BOARD_RPI3 = 0, LIGHT_BOARD_ESP32_CLASSIC = 1 } LIGHT_Board_t;
typedef struct
{
    const char *board_name;
    int sda_gpio, scl_gpio;
    int sda_physical, scl_physical;
    int power_physical, ground_physical; /* -1 where connector positions vary */
    uint8_t left_address, right_address;
    const char *linux_device; /* NULL on ESP32 */
} LIGHT_PinConfig_t;
const LIGHT_PinConfig_t *LIGHT_GetPinConfig(LIGHT_Board_t board);

#endif
