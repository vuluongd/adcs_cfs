/*
 * imu_app_processing.cpp - glue between cFS (C API) and the C++ gyro core.
 *
 * WAKEUP -> read MPU6050 -> real dt -> imu::Filter -> publish IMU_DATA.
 * Only this file touches the cFE API; imu::Filter and imu::Sensor have no cFS dependency.
 */
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "imu_filter.hpp"
#include "imu_sensor.hpp"

/* cFE headers are C: include them with C linkage. */
extern "C"
{
#include "imu_app.h"
#include "imu_app_eventids.h"
#include "imu_app_processing.h"
}

namespace
{

constexpr float         kDeg2Rad = 0.01745329252f;
constexpr std::uint32_t kPrintEvery = 10u; /* console line every N wakeups (1 Hz at 10 Hz wakeup) */

constexpr std::uint16_t kEidStart = 50;
constexpr std::uint16_t kEidTxErr = 51;
constexpr std::uint16_t kEidCalOk = 52;
constexpr std::uint16_t kEidCalFail = 53;
constexpr std::uint16_t kEidIo = 54;

class Processor
{
public:
    CFE_Status_t init();
    void         onWakeup();
    void         close() { sensor_.close(); }
    void         recalibrate() { filter_.recalibrate(); }
    void         resetAngle() { filter_.zeroAngle(); }

private:
    imu::Sensor   sensor_;
    imu::Filter   filter_;
    IMU_DataTlm_t tlm_{};
    std::uint32_t counter_ = 0;
    std::uint64_t lastNs_ = 0;
    bool          haveLast_ = false;
    std::uint8_t  lastStatus_ = 0xFF;
    std::uint32_t seenCalOk_ = 0, seenCalFail_ = 0;
};

Processor g_processor;

CFE_Status_t Processor::init()
{
    filter_.reset();
    sensor_.init();
    counter_ = 0;
    haveLast_ = false;
    lastStatus_ = 0xFF;
    seenCalOk_ = seenCalFail_ = 0;
    std::memset(&tlm_, 0, sizeof(tlm_));

    CFE_Status_t status =
        CFE_MSG_Init(CFE_MSG_PTR(tlm_.TelemetryHeader), CFE_SB_ValueToMsgId(IMU_APP_DATA_MID), sizeof(tlm_));
    if (status != CFE_SUCCESS) { return status; }
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(IMU_APP_WAKEUP_MID), IMU_APP_Data.CommandPipe);
    if (status == CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(kEidStart, CFE_EVS_EventType_INFORMATION,
                          "IMU backend=%s MPU6050 gyro Z; bias cal %.0fs; median3+LPF %.0fHz; dt from CLOCK_MONOTONIC",
                          sensor_.hardware() ? "I2C" : "SIMULATION", static_cast<double>(filter_.config().calTimeS),
                          static_cast<double>(filter_.config().lpfFcHz));
    }
    return status;
}

void Processor::onWakeup()
{
    float             raw = 0.0f, temp = 0.0f, dt = 0.0f;
    std::uint8_t      st = sensor_.read(raw, temp);
    imu::FilterOutput out;
    IMU_DataPayload_t &p = tlm_.Payload;

    if (st == IMU_STATUS_OK)
    {
        const std::uint64_t now = imu::monotonicNs();
        /* First sample after (re)start has dt = 0; after an I2C hole dt is large and the filter treats it as a gap. */
        dt = haveLast_ ? static_cast<float>(static_cast<double>(now - lastNs_) * 1e-9) : 0.0f;
        lastNs_ = now;
        haveLast_ = true;

        st = filter_.update(raw, dt, out);

        p.raw_rate_rad_s = raw * kDeg2Rad;
        p.rate_rad_s = out.rateDps * kDeg2Rad;
        p.angle_rad = out.angleDeg * kDeg2Rad;
        p.bias_rad_s = out.biasDps * kDeg2Rad;
        p.temp_c = temp;
        p.dt_s = dt;
        p.valid = out.valid;
        p.stationary = out.stationary;
    }
    else
    {
        /* Sensor not ready / I2C error: report it, keep the last numbers, never claim valid. */
        p.valid = 0;
        p.stationary = 0;
    }
    p.status = st;
    p.simulated = sensor_.hardware() ? 0 : 1;
    p.sample_counter = ++counter_;

    if (filter_.calOk() != seenCalOk_)
    {
        seenCalOk_ = filter_.calOk();
        CFE_EVS_SendEvent(kEidCalOk, CFE_EVS_EventType_INFORMATION,
                          "IMU bias calibrated: %.4f deg/s (sigma %.4f, attempts failed %lu)",
                          static_cast<double>(filter_.bias()), static_cast<double>(filter_.calSigma()),
                          static_cast<unsigned long>(filter_.calFail()));
    }
    if (filter_.calFail() != seenCalFail_)
    {
        seenCalFail_ = filter_.calFail();
        CFE_EVS_SendEvent(kEidCalFail, CFE_EVS_EventType_ERROR,
                          "IMU calibration rejected (moving or noisy, sigma %.3f deg/s); retrying. Keep platform still",
                          static_cast<double>(filter_.calSigma()));
    }
    if (st != lastStatus_ && (st == IMU_STATUS_IO_ERROR || st == IMU_STATUS_BAD_ID || st == IMU_STATUS_BAD_INPUT))
    {
        CFE_EVS_SendEvent(kEidIo, CFE_EVS_EventType_ERROR,
                          "IMU sensor status=%u (3=I2C error 4=bad WHO_AM_I 6=bad backend)",
                          static_cast<unsigned int>(st));
    }
    lastStatus_ = st;

    if (counter_ == 1 || counter_ % kPrintEvery == 0)
    {
        std::printf(">>> IMU [%u] raw=%.3f rate=%.3f deg/s bias=%.4f angle=%.2f deg T=%.1fC dt=%.3f stat=%d valid=%d "
                    "status=%d sim=%d\n",
                    static_cast<unsigned int>(p.sample_counter), static_cast<double>(p.raw_rate_rad_s / kDeg2Rad),
                    static_cast<double>(p.rate_rad_s / kDeg2Rad), static_cast<double>(p.bias_rad_s / kDeg2Rad),
                    static_cast<double>(p.angle_rad / kDeg2Rad), static_cast<double>(p.temp_c),
                    static_cast<double>(p.dt_s), p.stationary, p.valid, p.status, p.simulated);
    }

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(tlm_.TelemetryHeader));
    const CFE_Status_t tx = CFE_SB_TransmitMsg(CFE_MSG_PTR(tlm_.TelemetryHeader), true);
    if (tx != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(kEidTxErr, CFE_EVS_EventType_ERROR, "IMU_DATA transmit failed: %ld", static_cast<long>(tx));
    }
}

} // namespace

extern "C"
{

CFE_Status_t IMU_APP_ProcessingInit(void) { return g_processor.init(); }
void         IMU_APP_OnWakeup(void) { g_processor.onWakeup(); }
void         IMU_APP_ProcessingClose(void) { g_processor.close(); }
void         IMU_APP_Recalibrate(void) { g_processor.recalibrate(); }
void         IMU_APP_ResetAngle(void) { g_processor.resetAngle(); }

} // extern "C"
