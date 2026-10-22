/*
 * test_imu_filter.cpp - replay of a real MPU6050 log (t_us,raw_dps,temp_c) through imu::Filter.
 * Build: g++ -std=c++17 -O2 -I../fsw/src -I../fsw/inc test_imu_filter.cpp ../fsw/src/imu_filter.cpp -o test_imu_filter
 * Run  : ./test_imu_filter putty.log
 */
#include "imu_filter.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

namespace
{

constexpr double kPi = 3.14159265358979323846;

std::vector<double> T, W;
int                 fails = 0;

#define CHECK(cond, ...)                                                                                              \
    do                                                                                                                \
    {                                                                                                                 \
        if (!(cond)) { std::printf("  FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); fails++; }              \
        else         { std::printf("  ok  : "); std::printf(__VA_ARGS__); std::printf("\n"); }                        \
    } while (0)

void load(const char *path)
{
    std::FILE *fp = std::fopen(path, "r");
    char       line[256];
    if (!fp) { std::perror(path); std::exit(2); }
    while (std::fgets(line, sizeof line, fp))
    {
        unsigned long long t;
        double             w, tc;
        if (std::sscanf(line, "%llu,%lf,%lf", &t, &w, &tc) == 3)
        {
            T.push_back(static_cast<double>(t) * 1e-6);
            W.push_back(w);
        }
    }
    std::fclose(fp);
    const double t0 = T.front();
    for (double &t : T) { t -= t0; }
}

struct Res
{
    double   finalAngle = 0, maxAbsAngle = 0, bias = 0, statFrac = 0, calDoneT = 0;
    unsigned calOk = 0, calFail = 0, gaps = 0;
};

using InjFn = double (*)(double);  /* extra true rate (deg/s) added to the log */
using SkipFn = bool (*)(double);   /* true = sample missing */

Res run(std::size_t start, std::size_t decim, InjFn inj, SkipFn skip)
{
    auto              f = std::make_unique<imu::Filter>();
    imu::FilterOutput o;
    Res               r;
    double            lastT = -1;
    std::size_t       nst = 0, nrun = 0;
    bool              haveCal = false;

    for (std::size_t i = start; i < T.size(); i += decim)
    {
        const double t = T[i] - T[start];
        if (skip && skip(t)) { continue; }
        const double add = inj ? inj(t) : 0.0;
        const float  dt = lastT < 0 ? 0.f : static_cast<float>(t - lastT);
        lastT = t;
        f->update(static_cast<float>(W[i] + add), dt, o);
        if (!haveCal && !f->calibrating()) { r.calDoneT = t; haveCal = true; }
        if (!f->calibrating())
        {
            nrun++;
            if (o.stationary) { nst++; }
            if (std::fabs(o.angleDeg) > r.maxAbsAngle) { r.maxAbsAngle = std::fabs(o.angleDeg); }
        }
        r.finalAngle = o.angleDeg;
        r.bias = o.biasDps;
    }
    r.statFrac = nrun ? static_cast<double>(nst) / static_cast<double>(nrun) : 0;
    r.calOk = f->calOk();
    r.calFail = f->calFail();
    r.gaps = f->gaps();
    return r;
}

/* ---- injected disturbances (deg/s, t in seconds from start of run) ---- */
double injPulse(double t)      { return (t >= 20 && t < 24) ? 30.0 * std::sin(kPi * (t - 20) / 4.0) : 0.0; } /* truth +76.39 deg */
double injSlow(double t)       { return (t >= 30 && t < 50) ? 0.5 : 0.0; }                                   /* +10 deg          */
double injVerySlow(double t)   { return (t >= 30 && t < 50) ? 0.1 : 0.0; }                                   /* +2 deg           */
double injSpikeInCal(double t) { return (t >= 4.0 && t < 4.3) ? 20.0 : (t >= 6.0 && t < 6.02) ? -80.0 : 0.0; }
double injMovingStart(double t) { return t < 14.0 ? 5.0 * (1.0 - t / 14.0) : 0.0; }
bool   skipGap(double t) { return t >= 21.0 && t < 22.0; }

} // namespace

int main(int argc, char **argv)
{
    Res r;
    load(argc > 1 ? argv[1] : "putty.log");
    std::printf("Loaded %zu samples, %.1f s\n\n", T.size(), T.back());

    std::printf("[1] Replay raw log @100 Hz (device is still)\n");
    r = run(0, 1, nullptr, nullptr);
    std::printf("      bias=%.4f deg/s  final angle=%.3f deg  max|angle|=%.3f deg  stationary=%.1f%%  cal done at %.1fs\n",
                r.bias, r.finalAngle, r.maxAbsAngle, 100 * r.statFrac, r.calDoneT);
    CHECK(r.calOk == 1 && r.calFail == 0, "calibration accepted first try (ok=%u fail=%u)", r.calOk, r.calFail);
    CHECK(std::fabs(r.bias - 0.214) < 0.01, "bias estimate close to log mean 0.214 (%.4f)", r.bias);
    CHECK(r.maxAbsAngle < 0.5, "angle stays < 0.5 deg over %.0f s (max %.3f)", T.back(), r.maxAbsAngle);
    CHECK(r.statFrac > 0.90, "stationary detected %.1f%% of the time", 100 * r.statFrac);

    std::printf("\n[2] Same log decimated to 10 Hz (sch_lab TickRate=10)\n");
    r = run(0, 10, nullptr, nullptr);
    std::printf("      bias=%.4f  final=%.3f  max|angle|=%.3f  stationary=%.1f%%\n", r.bias, r.finalAngle, r.maxAbsAngle,
                100 * r.statFrac);
    CHECK(r.maxAbsAngle < 0.5 && r.calFail == 0, "10 Hz also stable (max %.3f deg)", r.maxAbsAngle);

    std::printf("\n[3] Shocks inside calibration window (+20 deg/s for 0.3 s, -80 deg/s for 20 ms)\n");
    r = run(0, 1, injSpikeInCal, nullptr);
    std::printf("      bias=%.4f ok=%u fail=%u final=%.3f\n", r.bias, r.calOk, r.calFail, r.finalAngle);
    CHECK(r.calOk >= 1 && std::fabs(r.bias - 0.214) < 0.02, "bias survives outliers (%.4f)", r.bias);

    std::printf("\n[4] Platform still turning at start (5 deg/s decaying to 0 at 14 s)\n");
    r = run(0, 1, injMovingStart, nullptr);
    std::printf("      first accepted calibration at %.1fs, fail=%u, bias=%.4f\n", r.calDoneT, r.calFail, r.bias);
    CHECK(r.calFail >= 1 && std::fabs(r.bias - 0.214) < 0.02, "moving window rejected, retried, bias=%.4f", r.bias);

    std::printf("\n[5] Real rotation tracking (injected on the real noisy log)\n");
    r = run(0, 1, injPulse, nullptr);
    CHECK(std::fabs(r.finalAngle - 76.39) < 1.0, "30 deg/s half-sine pulse @100 Hz: %.2f deg (truth 76.39)", r.finalAngle);
    r = run(0, 10, injPulse, nullptr);
    CHECK(std::fabs(r.finalAngle - 76.39) < 1.5, "same pulse @10 Hz: %.2f deg (truth 76.39)", r.finalAngle);
    r = run(0, 1, injSlow, nullptr);
    CHECK(std::fabs(r.finalAngle - 10.0) < 0.5, "0.5 deg/s for 20 s: %.2f deg (truth 10.0)", r.finalAngle);
    r = run(0, 1, injVerySlow, nullptr);
    CHECK(r.finalAngle > 1.8 && r.finalAngle < 2.6, "0.1 deg/s for 20 s: %.2f deg (truth 2.00, deadband 0.08)", r.finalAngle);

    std::printf("\n[6] 1 s sample dropout in the middle of a pulse\n");
    r = run(0, 1, injPulse, skipGap);
    std::printf("      angle=%.2f deg (truth 76.39, lost area ~ the 1 s hole), gaps=%u\n", r.finalAngle, r.gaps);
    CHECK(r.gaps == 1 && r.finalAngle > 40 && r.finalAngle < 80, "no blow-up across the gap");

    std::printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
