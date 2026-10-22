/*
 * imu_sensor.hpp - MPU6050 gyro Z reader (Linux i2c-dev) with a simulation backend.
 * Never sleeps: initialisation is a small state machine advanced by successive read() calls.
 */
#ifndef IMU_SENSOR_HPP
#define IMU_SENSOR_HPP

#include <cstdint>

namespace imu
{

std::uint64_t monotonicNs(); /* CLOCK_MONOTONIC in nanoseconds */

class Sensor
{
public:
    Sensor() = default;
    ~Sensor() { close(); }
    Sensor(const Sensor &) = delete;
    Sensor &operator=(const Sensor &) = delete;

    /* Reads env IMU_APP_BACKEND: unset or "sim" = simulation, "i2c" = MPU6050, anything else = invalid. */
    void init();
    void close();

    /* Returns IMU_STATUS_*. On IMU_STATUS_OK, gzDps / tempC are valid. */
    std::uint8_t read(float &gzDps, float &tempC);

    bool         hardware() const { return hardware_; }
    std::uint8_t whoAmI() const { return who_; }

private:
    enum class Stage : std::uint8_t { Closed, Woken, Configured, Running };

    std::uint8_t simRead(float &gz, float &temp);

    int           fd_ = -1;
    bool          hardware_ = false; /* false = simulation */
    Stage         stage_ = Stage::Closed;
    std::uint8_t  who_ = 0;
    std::uint64_t readyAfterMs_ = 0;
    std::uint64_t simT0Ms_ = 0;
    std::uint32_t lcg_ = 12345u;
};

} // namespace imu

#endif
