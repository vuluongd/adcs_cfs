/*
 * imu_filter.hpp - Gyro Z processing core for IMU_APP (C++17, no cFS dependency).
 *
 * Pipeline (project slide 42):
 *   1. Bias calibration  : robust mean of the first calTimeS (median/MAD outlier rejection)
 *   2. Median filter     : 3 samples, removes single-sample spikes
 *   3. Low-pass filter   : 1st order, cutoff lpfFcHz, alpha computed from the REAL dt
 *   4. Stationary detect : |rate| < statThreshDps for statTimeS -> freeze angle, adapt bias slowly
 *   5. Integration       : trapezoid with real dt, never integrates across a sampling gap
 *
 * Units: input/output in deg/s and deg; the cFS layer converts to rad.
 * No heap, no exceptions, no RTTI: safe for flight-style builds (-fno-exceptions -fno-rtti).
 */
#ifndef IMU_FILTER_HPP
#define IMU_FILTER_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "imu_status.h"

namespace imu
{

struct FilterConfig
{
    float         calTimeS         = 10.0f; /* calibration window length                          */
    std::uint32_t calMinSamples    = 20;    /* minimum samples before a window may close          */
    float         calOutlierK      = 5.0f;  /* reject |x-median| > k*sigmaMAD                     */
    float         calSigmaFloorDps = 0.01f; /* floor for sigmaMAD (gyro LSB is 0.0076 deg/s)      */
    float         calMaxSigmaDps   = 0.10f; /* fail calibration if inlier std is larger           */
    float         calMaxAbsBiasDps = 3.0f;  /* fail calibration if |bias| is larger               */
    float         calMaxHalfDiffDps = 0.10f; /* fail if 1st/2nd half means differ (moving/warming) */
    float         calMinInlierFrac = 0.80f; /* fail if too many outliers                          */
    float         lpfFcHz          = 10.0f; /* low-pass cutoff                                    */
    float         statThreshDps    = 0.08f; /* stationary deadband on filtered rate               */
    float         statTimeS        = 1.0f;  /* time inside the deadband before "stationary"       */
    float         biasTauS         = 30.0f; /* bias adaptation time constant while stationary     */
    float         maxDtS           = 0.25f; /* dt above this is a gap: do not integrate over it   */
    float         satDps           = 249.0f; /* |raw| >= this is treated as saturated/invalid     */
};

struct FilterOutput
{
    float        rateDps    = 0.0f; /* filtered, bias-free angular rate          */
    float        angleDeg   = 0.0f; /* integrated angle (relative to last zero)  */
    float        biasDps    = 0.0f; /* current bias estimate                     */
    std::uint8_t valid      = 0;    /* 1 when calibrated and sample accepted     */
    std::uint8_t status     = 0;    /* IMU_STATUS_*                              */
    std::uint8_t stationary = 0;    /* 1 while angle is frozen / bias adapting   */
};

class Filter
{
public:
    static constexpr std::size_t kCalBufMax = 1024;

    explicit Filter(const FilterConfig &cfg = FilterConfig{}) { reset(cfg); }

    void reset(const FilterConfig &cfg = FilterConfig{}); /* full reset: counters, bias, angle, restart calibration */
    void recalibrate();                                   /* restart bias calibration, keep angle                   */
    void zeroAngle() { angle_ = 0.0; }

    /* Feed one gyro sample. Returns IMU_STATUS_*; also stored in out.status. */
    std::uint8_t update(float rawDps, float dtS, FilterOutput &out);

    const FilterConfig &config() const { return cfg_; }
    bool                calibrating() const { return calibrating_; }
    float               bias() const { return bias_; }
    float               calSigma() const { return calSigma_; }
    std::uint32_t       samples() const { return samples_; }
    std::uint32_t       rejected() const { return rejected_; }
    std::uint32_t       gaps() const { return gaps_; }
    std::uint32_t       calOk() const { return calOk_; }
    std::uint32_t       calFail() const { return calFail_; }

private:
    void restartRun();
    bool finishCalibration();
    void stepCalibration(float raw, float dt, FilterOutput &out);
    void stepRun(float raw, float dt);
    void fill(FilterOutput &out) const;

    FilterConfig                        cfg_;
    bool                                calibrating_ = true;
    std::array<float, kCalBufMax>       calBuf_{};
    std::uint32_t                       calN_ = 0;
    float                               calElapsed_ = 0.0f;
    std::array<float, 3>                med_{};
    std::uint32_t                       medN_ = 0;
    bool                                lpfInit_ = false;
    bool                                havePrev_ = false;
    float                               y_ = 0.0f, prevY_ = 0.0f;
    float                               bias_ = 0.0f;
    double                              angle_ = 0.0;
    float                               statTimer_ = 0.0f;
    bool                                stationary_ = false;
    std::uint32_t                       samples_ = 0, rejected_ = 0, gaps_ = 0, calOk_ = 0, calFail_ = 0;
    float                               calSigma_ = 0.0f;
};

} // namespace imu

#endif
