#define _POSIX_C_SOURCE 200809L
#include "light_sensor.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

static uint64_t LIGHT_MonotonicMs(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static int LIGHT_WriteCommand(int fd, uint8_t cmd)
{
    ssize_t n;
    do { n = write(fd, &cmd, 1); } while (n < 0 && errno == EINTR);
    return n == 1 ? 0 : -1;
}

static int LIGHT_OpenOne(const char *path, int address)
{
    int fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return -1;
    /* Separate descriptors: each holds its own slave address. Default MTreg=69. */
    if (ioctl(fd, I2C_SLAVE, address) < 0 || LIGHT_WriteCommand(fd, 0x01) < 0 ||
        LIGHT_WriteCommand(fd, 0x07) < 0 || LIGHT_WriteCommand(fd, 0x42) < 0 ||
        LIGHT_WriteCommand(fd, 0x65) < 0 || LIGHT_WriteCommand(fd, 0x10) < 0)
    {
        close(fd);
        return -1;
    }
    return fd;
}

void LIGHT_SensorClose(LIGHT_Sensor_t *s)
{
    if (s->fd_left >= 0) close(s->fd_left);
    if (s->fd_right >= 0) close(s->fd_right);
    s->fd_left = s->fd_right = -1;
}

void LIGHT_SensorInit(LIGHT_Sensor_t *s)
{
    const char *backend = getenv("LIGHT_APP_BACKEND");
    memset(s, 0, sizeof(*s));
    s->fd_left = s->fd_right = -1;
    /* Unknown backend is treated as hardware to fail invalid, never silently simulate. */
    s->hardware = backend != NULL && strcmp(backend, "sim") != 0;
}

static uint8_t LIGHT_ReadOne(int fd, float *lux)
{
    uint8_t b[2];
    uint16_t raw;
    ssize_t n;
    do { n = read(fd, b, sizeof(b)); } while (n < 0 && errno == EINTR);
    if (n != 2)
        return LIGHT_STATUS_IO_ERROR;
    raw = (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
    *lux = (float)raw / 1.2f; /* Continuous H-resolution mode1, MTreg=69 */
    return raw >= 65500u ? LIGHT_STATUS_SATURATED : LIGHT_STATUS_OK;
}

uint8_t LIGHT_SensorRead(LIGHT_Sensor_t *s, float *left, float *right)
{
    const char *path, *backend;
    const LIGHT_PinConfig_t *pins=LIGHT_GetPinConfig(LIGHT_BOARD_RPI3);
    uint8_t ls, rs;
    *left = *right = 0.0f;
    if (!s->hardware)
    {
        return LIGHT_SimGenerate(s->sim_step++,LIGHT_SIM_SWEEP,left,right,NULL);
    }
    backend = getenv("LIGHT_APP_BACKEND");
    if (backend == NULL || strcmp(backend, "i2c") != 0)
        return LIGHT_STATUS_BAD_INPUT;
    if (s->fd_left < 0 || s->fd_right < 0)
    {
        LIGHT_SensorClose(s);
        path = getenv("LIGHT_APP_I2C_DEVICE");
        if (path == NULL) path = pins->linux_device;
        s->fd_left = LIGHT_OpenOne(path, pins->left_address);
        s->fd_right = LIGHT_OpenOne(path, pins->right_address);
        if (s->fd_left < 0 || s->fd_right < 0)
        {
            LIGHT_SensorClose(s);
            return LIGHT_STATUS_IO_ERROR;
        }
        /* No sleep in the cFS wakeup handler. First sample waits for next wakeup. */
        s->ready_after_ms = LIGHT_MonotonicMs() + 180u;
        return LIGHT_STATUS_NOT_READY;
    }
    if (LIGHT_MonotonicMs() < s->ready_after_ms)
        return LIGHT_STATUS_NOT_READY;
    ls = LIGHT_ReadOne(s->fd_left, left);
    rs = LIGHT_ReadOne(s->fd_right, right);
    if (ls == LIGHT_STATUS_IO_ERROR || rs == LIGHT_STATUS_IO_ERROR)
    {
        LIGHT_SensorClose(s); /* Retry initialization on the next wakeup. */
        return LIGHT_STATUS_IO_ERROR;
    }
    if (ls != LIGHT_STATUS_OK) return ls;
    return rs;
}
