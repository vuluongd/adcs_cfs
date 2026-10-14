# V2 shadow-baffle validation — 2026-10-12

Source baseline: vuluongd/adcs_cfs commit 91ae348df26ef91388310a43a26edcc18d3f7702.
Linux GCC 13.3, CMake 4.4.4. The sensor geometry is now parallel sensors with
a central baffle. The tilted-sensor atan model was removed.

Verified:
- Strict C99 standalone build (-Wall -Wextra -Werror -pedantic).
- Three repository CTest suites passed: estimator, mocked Linux I2C boundary,
  mocked ADCS receiver. The delivered standalone package contains the first two.
- 105 normalized-difference/intensity combinations including one shaded channel.
- No calibrated table => error_valid=1, angle_valid=0, status=NO_CALIBRATION.
- Gain/offset balance, dark/NaN/negative input, transport/not-ready/saturation
  failures, EMA and reset after invalid samples.
- Synthetic asymmetric LUT interpolation, endpoints, no extrapolation,
  repeated-point/bad-count rejection and changed-table filter-range rejection.
  The synthetic table exists ONLY in tests, never in the operational defaults.
- ADCS distinguishes valid dimensionless error from valid angle, rejects stale
  and future timestamps, malformed packets, nonfinite error and simulated data
  unless explicitly permitted.
- Full native non-EDS cFS mission-all build passed with the updated app sources.
- CLI input 400/600 lux gives error=+0.2000, error_valid=1, angle_valid=0,
  status=7, angle=N/A.

Not verified:
- No physical sensor, baffle, Raspberry Pi or reaction wheel connected.
- No measured angular accuracy and no user's calibration points available.
- No complete closed-loop PID/PWM/IMU control, no EDS, no full upstream suite.
- V2 runtime was not rerun on the hosted cFS process; native runtime transmission
  was demonstrated on V1. V2 was rebuilt against actual cFE, and its updated
  receiver semantics were verified by the focused tests.

This package provides light-error data now, and calibrated angle data only
after real monotonic calibration points are supplied.
