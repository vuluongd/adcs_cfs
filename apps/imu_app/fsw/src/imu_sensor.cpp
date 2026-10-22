#include "imu_sensor.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "imu_status.h"

namespace imu
{

namespace
{

/* MPU6050 registers / settings: identical to ghi_log_raw.ino so the log and the flight code agree. */
constexpr std::uint8_t kAddr = 0x68; /* AD0 = GND (slide 40) */
constexpr std::uint8_t kRegSmplrtDiv = 0x19;
constexpr std::uint8_t kRegConfig = 0x1A;
constexpr std::uint8_t kRegGyroCfg = 0x1B;
constexpr std::uint8_t kRegTempOutH = 0x41;
constexpr std::uint8_t kRegPwrMgmt1 = 0x6B;
constexpr std::uint8_t kRegWhoAmI = 0x75;
constexpr float        kGyroSens = 131.0f; /* LSB per deg/s at +-250 deg/s */
constexpr double       kPi = 3.14159265358979;

std::uint64_t monotonicMs() { return monotonicNs() / 1000000ull; }

bool writeReg(int fd, std::uint8_t reg, std::uint8_t val)
{
    const std::uint8_t b[2] = {reg, val};
    ssize_t            n;
    do { n = ::write(fd, b, 2); } while (n < 0 && errno == EINTR);
    return n == 2;
}

/* Register burst read with a repeated start (single I2C transaction). */
bool readRegs(int fd, std::uint8_t reg, std::uint8_t *buf, std::uint16_t len)
{
    struct i2c_msg msgs[2];
    std::memset(msgs, 0, sizeof(msgs));
    msgs[0].addr = kAddr;
    msgs[0].flags = 0;
    msgs[0].len = 1;
    msgs[0].buf = &reg;
    msgs[1].addr = kAddr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = len;
    msgs[1].buf = buf;

    struct i2c_rdwr_ioctl_data d;
    d.msgs = msgs;
    d.nmsgs = 2;
    int rc;
    do { rc = ::ioctl(fd, I2C_RDWR, &d); } while (rc < 0 && errno == EINTR);
    return rc >= 0;
}

} // namespace

std::uint64_t monotonicNs()
{
    struct timespec ts;
    if (::clock_gettime(CLOCK_MONOTONIC, &ts) != 0) { return 0; }
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ull + static_cast<std::uint64_t>(ts.tv_nsec);
}

void Sensor::close()
{
    if (fd_ >= 0) { ::close(fd_); }
    fd_ = -1;
    stage_ = Stage::Closed;
}

void Sensor::init()
{
    const char *backend = std::getenv("IMU_APP_BACKEND");
    close();
    who_ = 0;
    readyAfterMs_ = 0;
    lcg_ = 12345u;
    /* Unknown backend is treated as hardware so it fails invalid; never silently simulates. */
    hardware_ = backend != nullptr && std::strcmp(backend, "sim") != 0;
    simT0Ms_ = monotonicMs();
}

/* Simulation: bias 0.21 deg/s + noise 0.03 deg/s (measured on the real log) and a 30 deg/s half-sine
 * pulse lasting 4 s every 40 s starting at t = 25 s, so ground telemetry shows a ~76.4 deg turn. */
std::uint8_t Sensor::simRead(float &gz, float &temp)
{
    const double t = static_cast<double>(monotonicMs() - simT0Ms_) / 1000.0;
    const double ph = std::fmod(t, 40.0);
    double       n = 0.0, rate = 0.0;
    for (int i = 0; i < 12; i++)
    {
        lcg_ = lcg_ * 1664525u + 1013904223u;
        n += static_cast<double>(lcg_ >> 8) / 16777216.0;
    }
    n = (n - 6.0) * 0.03; /* ~N(0, 0.03) */
    if (t >= 25.0 && ph >= 25.0 && ph < 29.0) { rate = 30.0 * std::sin(kPi * (ph - 25.0) / 4.0); }
    gz = static_cast<float>(0.21 + n + rate);
    temp = 30.5f;
    return IMU_STATUS_OK;
}

std::uint8_t Sensor::read(float &gz, float &temp)
{
    gz = 0.0f;
    temp = 0.0f;
    if (!hardware_) { return simRead(gz, temp); }

    const char *backend = std::getenv("IMU_APP_BACKEND");
    if (backend == nullptr || std::strcmp(backend, "i2c") != 0) { return IMU_STATUS_BAD_INPUT; }
    const std::uint64_t now = monotonicMs();

    if (stage_ == Stage::Closed)
    {
        const char *path = std::getenv("IMU_APP_I2C_DEVICE");
        if (path == nullptr) { path = "/dev/i2c-1"; }
        fd_ = ::open(path, O_RDWR | O_CLOEXEC);
        if (fd_ < 0) { return IMU_STATUS_IO_ERROR; }
        if (::ioctl(fd_, I2C_SLAVE, kAddr) < 0 || !readRegs(fd_, kRegWhoAmI, &who_, 1))
        {
            close();
            return IMU_STATUS_IO_ERROR;
        }
        if (who_ == 0x00 || who_ == 0xFF) { close(); return IMU_STATUS_BAD_ID; }
        if (!writeReg(fd_, kRegPwrMgmt1, 0x01)) { close(); return IMU_STATUS_IO_ERROR; }
        stage_ = Stage::Woken;
        readyAfterMs_ = now + 100u; /* wake-up settle, same 100 ms as the .ino */
        return IMU_STATUS_NOT_READY;
    }
    if (stage_ == Stage::Woken)
    {
        if (now < readyAfterMs_) { return IMU_STATUS_NOT_READY; }
        if (!writeReg(fd_, kRegConfig, 0x03) ||    /* DLPF ~42 Hz        */
            !writeReg(fd_, kRegGyroCfg, 0x00) ||   /* +-250 deg/s        */
            !writeReg(fd_, kRegSmplrtDiv, 9))      /* 100 Hz output rate */
        {
            close();
            return IMU_STATUS_IO_ERROR;
        }
        stage_ = Stage::Configured;
        readyAfterMs_ = now + 500u; /* gyro settle, same 500 ms as the .ino */
        return IMU_STATUS_NOT_READY;
    }
    if (stage_ == Stage::Configured)
    {
        if (now < readyAfterMs_) { return IMU_STATUS_NOT_READY; }
        stage_ = Stage::Running;
    }

    /* One burst: TEMP_H,TEMP_L,GX_H,GX_L,GY_H,GY_L,GZ_H,GZ_L (regs 0x41..0x48) -> temp and gyro Z consistent */
    std::uint8_t b[8];
    if (!readRegs(fd_, kRegTempOutH, b, 8))
    {
        close(); /* retry initialization on the next wakeup */
        return IMU_STATUS_IO_ERROR;
    }
    const std::int16_t rawT = static_cast<std::int16_t>((static_cast<std::uint16_t>(b[0]) << 8) | b[1]);
    const std::int16_t rawG = static_cast<std::int16_t>((static_cast<std::uint16_t>(b[6]) << 8) | b[7]);
    temp = static_cast<float>(rawT) / 340.0f + 36.53f;
    gz = static_cast<float>(rawG) / kGyroSens;
    return IMU_STATUS_OK;
}

} // namespace imu
