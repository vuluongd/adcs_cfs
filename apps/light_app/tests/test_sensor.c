#define _POSIX_C_SOURCE 200809L
#include "light_sensor.h"
#include <assert.h>
#include <linux/i2c-dev.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

/* Test doubles for the Linux boundary. No real bus accessed. */
static int opened, closed, addresses[2], writes[2], short_read, fail_right;
static uint16_t raw[2] = {1200, 2400};
static uint64_t now_ms = 1000;

int __wrap_open(const char *path, int flags, ...)
{
    (void)path; (void)flags;
    if (fail_right && opened == 1) return -1;
    return 10 + opened++;
}
int __wrap_ioctl(int fd, unsigned long request, ...)
{
    va_list ap;
    assert(request == I2C_SLAVE && fd >= 10 && fd <= 11);
    va_start(ap, request);
    addresses[fd - 10] = va_arg(ap, int);
    va_end(ap);
    return 0;
}
ssize_t __wrap_write(int fd, const void *buf, size_t count)
{
    const uint8_t commands[] = {0x01, 0x07, 0x42, 0x65, 0x10};
    assert(fd >= 10 && fd <= 11 && count == 1);
    assert(writes[fd - 10] < 5);
    assert(*(const uint8_t *)buf == commands[writes[fd - 10]++]);
    return 1;
}
ssize_t __wrap_read(int fd, void *buf, size_t count)
{
    uint8_t *b = buf;
    assert(fd >= 10 && fd <= 11 && count == 2);
    if (short_read) return 1;
    b[0] = (uint8_t)(raw[fd - 10] >> 8);
    b[1] = (uint8_t)raw[fd - 10];
    return 2;
}
int __wrap_close(int fd)
{
    assert(fd >= 10 && fd <= 11);
    ++closed;
    return 0;
}
int __wrap_clock_gettime(clockid_t id, struct timespec *ts)
{
    assert(id == CLOCK_MONOTONIC);
    ts->tv_sec = (time_t)(now_ms / 1000);
    ts->tv_nsec = (long)(now_ms % 1000) * 1000000;
    return 0;
}
static void reset_boundary(void)
{
    opened = closed = 0;
    writes[0] = writes[1] = 0;
    addresses[0] = addresses[1] = 0;
    short_read = fail_right = 0;
}

int main(void)
{
    LIGHT_Sensor_t s;
    float l, r;
    assert(setenv("LIGHT_APP_BACKEND", "i2c", 1) == 0);
    LIGHT_SensorInit(&s);
    assert(s.hardware);
    assert(LIGHT_SensorRead(&s, &l, &r) == LIGHT_STATUS_NOT_READY);
    assert(addresses[0] == 0x23 && addresses[1] == 0x5c);
    assert(writes[0] == 5 && writes[1] == 5);
    now_ms = 1179;
    assert(LIGHT_SensorRead(&s, &l, &r) == LIGHT_STATUS_NOT_READY);
    now_ms = 1180;
    assert(LIGHT_SensorRead(&s, &l, &r) == LIGHT_STATUS_OK);
    assert(fabsf(l - 1000.0f) < 0.001f && fabsf(r - 2000.0f) < 0.001f);
    raw[1] = 65535;
    assert(LIGHT_SensorRead(&s, &l, &r) == LIGHT_STATUS_SATURATED);
    raw[1] = 2400;
    short_read = 1;
    assert(LIGHT_SensorRead(&s, &l, &r) == LIGHT_STATUS_IO_ERROR);
    assert(s.fd_left == -1 && s.fd_right == -1 && closed == 2);
    reset_boundary();
    assert(LIGHT_SensorRead(&s, &l, &r) == LIGHT_STATUS_NOT_READY);
    now_ms += 200;
    assert(LIGHT_SensorRead(&s, &l, &r) == LIGHT_STATUS_OK);
    LIGHT_SensorClose(&s);
    reset_boundary(); fail_right = 1;
    LIGHT_SensorInit(&s);
    assert(LIGHT_SensorRead(&s, &l, &r) == LIGHT_STATUS_IO_ERROR);
    assert(closed == 1 && s.fd_left == -1 && s.fd_right == -1);
    assert(setenv("LIGHT_APP_BACKEND", "typo", 1) == 0);
    LIGHT_SensorInit(&s);
    assert(LIGHT_SensorRead(&s, &l, &r) == LIGHT_STATUS_BAD_INPUT);
    puts("PASS: addresses, commands/MTreg, readiness, byte order, saturation, short read, reconnect, partial init");
    return 0;
}
