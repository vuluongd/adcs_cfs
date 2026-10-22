#include "imu_filter.hpp"

#include <algorithm>
#include <cmath>

namespace imu
{

namespace
{

/* Median of v[0..n), sorts in place. */
float medianOf(float *v, std::uint32_t n)
{
    std::sort(v, v + n);
    return (n & 1u) ? v[n / 2] : 0.5f * (v[n / 2 - 1] + v[n / 2]);
}

float median3(float a, float b, float c)
{
    if (a > b) { std::swap(a, b); }
    if (b > c) { b = c; }
    return a > b ? a : b;
}

} // namespace

void Filter::reset(const FilterConfig &cfg)
{
    cfg_ = cfg;
    bias_ = 0.0f;
    angle_ = 0.0;
    samples_ = rejected_ = gaps_ = calOk_ = calFail_ = 0;
    calSigma_ = 0.0f;
    recalibrate();
}

void Filter::restartRun()
{
    medN_ = 0;
    lpfInit_ = false;
    havePrev_ = false;
    statTimer_ = 0.0f;
    stationary_ = false;
    y_ = prevY_ = 0.0f;
}

void Filter::recalibrate()
{
    calibrating_ = true;
    calN_ = 0;
    calElapsed_ = 0.0f;
    restartRun();
}

/* Close the calibration window. Returns true when the bias was accepted. */
bool Filter::finishCalibration()
{
    const FilterConfig &c = cfg_;
    std::array<float, kCalBufMax> tmp;
    double        s = 0.0, s2 = 0.0, a1 = 0.0, a2 = 0.0;
    std::uint32_t nin = 0, n1 = 0, n2 = 0;
    const std::uint32_t n = calN_;

    std::copy(calBuf_.begin(), calBuf_.begin() + n, tmp.begin());
    const float med = medianOf(tmp.data(), n);
    for (std::uint32_t i = 0; i < n; i++) { tmp[i] = std::fabs(calBuf_[i] - med); }
    const float mad = medianOf(tmp.data(), n);
    float       sigma = 1.4826f * mad;
    if (sigma < c.calSigmaFloorDps) { sigma = c.calSigmaFloorDps; }
    const float thr = c.calOutlierK * sigma;

    for (std::uint32_t i = 0; i < n; i++)
    {
        const float x = calBuf_[i];
        if (std::fabs(x - med) > thr) { continue; }
        s += x;
        s2 += static_cast<double>(x) * x;
        nin++;
        if (i < n / 2) { a1 += x; n1++; } else { a2 += x; n2++; }
    }
    if (nin < 2 || n1 == 0 || n2 == 0) { return false; }
    const float mean = static_cast<float>(s / nin);
    const float stdv = static_cast<float>(std::sqrt(std::fmax(0.0, s2 / nin - static_cast<double>(mean) * mean)));
    const float m1 = static_cast<float>(a1 / n1);
    const float m2 = static_cast<float>(a2 / n2);
    calSigma_ = stdv;

    if (static_cast<float>(nin) / static_cast<float>(n) < c.calMinInlierFrac) { return false; }
    if (stdv > c.calMaxSigmaDps) { return false; }
    if (std::fabs(mean) > c.calMaxAbsBiasDps) { return false; }
    if (std::fabs(m1 - m2) > c.calMaxHalfDiffDps) { return false; }

    bias_ = mean;
    return true;
}

void Filter::stepCalibration(float raw, float dt, FilterOutput &out)
{
    if (calN_ < kCalBufMax) { calBuf_[calN_++] = raw; }
    calElapsed_ += dt;
    if ((calElapsed_ >= cfg_.calTimeS && calN_ >= cfg_.calMinSamples) || calN_ >= kCalBufMax)
    {
        if (finishCalibration())
        {
            calOk_++;
            calibrating_ = false;
            restartRun();
        }
        else
        {
            calFail_++;
            calN_ = 0;
            calElapsed_ = 0.0f;
        }
    }
    /* The sample that closes the window is not used for integration. */
    out.status = calibrating_ ? IMU_STATUS_CALIBRATING : IMU_STATUS_OK;
}

void Filter::stepRun(float raw, float dt)
{
    const FilterConfig &c = cfg_;

    /* 2. median of 3 */
    if (medN_ < 3) { med_[medN_++] = raw; }
    else { med_[0] = med_[1]; med_[1] = med_[2]; med_[2] = raw; }
    const float med = (medN_ >= 3) ? median3(med_[0], med_[1], med_[2]) : raw;
    const float x = med - bias_;

    /* 3. first-order low-pass with real dt */
    if (!lpfInit_)
    {
        y_ = x;
        lpfInit_ = true;
    }
    else if (dt > 0.0f)
    {
        const float rc = 1.0f / (6.28318531f * c.lpfFcHz);
        const float a = dt / (rc + dt);
        y_ += a * (x - y_);
    }

    /* 4. stationary detection */
    if (std::fabs(y_) < c.statThreshDps) { statTimer_ += dt; } else { statTimer_ = 0.0f; }
    stationary_ = statTimer_ >= c.statTimeS;

    if (stationary_)
    {
        float a = dt / c.biasTauS;
        if (a > 1.0f) { a = 1.0f; }
        bias_ += a * x; /* slow bias tracking, angle frozen */
    }
    else if (havePrev_ && dt > 0.0f)
    {
        angle_ += 0.5 * (static_cast<double>(prevY_) + static_cast<double>(y_)) * static_cast<double>(dt); /* 5. trapezoid */
    }
    prevY_ = y_;
    havePrev_ = true;
}

void Filter::fill(FilterOutput &out) const
{
    out.rateDps = y_;
    out.angleDeg = static_cast<float>(angle_);
    out.biasDps = bias_;
    out.stationary = stationary_ ? 1 : 0;
}

std::uint8_t Filter::update(float raw, float dt, FilterOutput &out)
{
    out.valid = 0;
    out.status = IMU_STATUS_OK;
    samples_++;

    if (!std::isfinite(raw) || std::fabs(raw) >= cfg_.satDps)
    {
        rejected_++;
        out.status = IMU_STATUS_SATURATED;
        fill(out);
        return out.status;
    }
    if (!(dt > 0.0f)) { dt = 0.0f; }
    if (dt > cfg_.maxDtS)
    {
        gaps_++;
        dt = 0.0f;
        medN_ = 0; /* old samples are no longer neighbours */
        lpfInit_ = false;
        havePrev_ = false; /* do not integrate across the hole */
        statTimer_ = 0.0f;
    }

    if (calibrating_)
    {
        stepCalibration(raw, dt, out);
    }
    else
    {
        stepRun(raw, dt);
        out.valid = 1;
    }
    fill(out);
    return out.status;
}

} // namespace imu
