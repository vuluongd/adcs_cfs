# Validation — 2026-10-12

Base repository: vuluongd/adcs_cfs, 91ae348df26ef91388310a43a26edcc18d3f7702.
Host: Linux, GCC 13.3, CMake 4.4.4, cFE submodule f22e538cafe56cf44b15996b67bd1e90850a30cf.

## Verified

- Standalone C99 compile with -Wall -Wextra -Werror -pedantic.
- Three repository native CTest executables passed. The independent standalone
  package was rebuilt from its delivered sources and its two included tests passed.
- The complete patch passed git apply --check on a separate checkout of the base commit.
- 345 ideal cosine angle/intensity cases (-34 to +34deg, five intensities).
- Equal readings, gain and offset correction, darkness, out-of-view rejection,
  saturation/error/not-ready propagation, NaN/negative input, bad config, EMA,
  filter reset after an invalid sample, simulated backend.
- Linux I2C boundary doubles verified addresses 0x23/0x5c, power/reset/MTreg/mode
  commands, 180ms readiness, big-endian reading and raw/1.2 conversion, clipping,
  short read, recovery, failed second-device initialization and unknown backend.
- ADCS receiver boundary test verified valid reception, 500ms accepted/700ms
  rejected, future timestamp rejected, simulation gate, invalid status/NaN,
  malformed packet size and timestamp failure.
- Estimator and ADCS receiver also ran with ASan/UBSan; LeakSanitizer disabled
  because the hosted environment prevents its process inspection.
- Full non-EDS native cFS mission built and installed. The cFE upstream build
  emitted an existing snprintf truncation warning in cfe_evs_utils.c.
- Actual cFS scheduler/software-bus run delivered LIGHT_DATA to ADCS_APP:

```
sample=1  bearing=0.00deg   valid=1 status=0 simulated=1
sample=10 bearing=14.85deg  valid=1 status=0 simulated=1
sample=20 bearing=24.63deg  valid=1 status=0 simulated=1
sample=30 bearing=19.48deg  valid=1 status=0 simulated=1
sample=40 bearing=2.50deg   valid=1 status=0 simulated=1
sample=50 bearing=-15.98deg valid=1 status=0 simulated=1
```

Samples 10/20/30/40/50 were logged two seconds apart, matching the configured
5Hz LIGHT wakeup and logging each tenth packet. The run was intentionally ended
with timeout/SIGTERM, after entering OPERATIONAL and receiving over 50 samples.
The start-up messages saying no subscriber for event telemetry MID 0x808 are
expected in the small mission without TO_LAB.

## Hosted-runtime adaptation

This host runs UID 0 without the capability needed to exceed the Linux mqueue
depth limit of 10. OSAL's standard permissive native path truncates queues only
when UID is nonzero. For the runtime test only, the local OSAL BSP was adjusted
to read that queue limit when CFS_TEST_LIMIT_QUEUES was set. This changed queue
capacity, not sensor values, app logic or software-bus messages. The dependency
source was restored afterward; the change is not included in the delivered patch.
The native mission also enables the repository's normal OSAL permissive config.
On a normal Linux user account, OSAL already reads the queue limit. The delivered
guide targets that ordinary account workflow, not privileged container execution.

## Not verified

- No physical BH1750, Raspberry Pi or reaction wheel connected.
- No measured angular accuracy: all angle ground truth is synthetic cosine data.
- No closed-loop plant/gyro/PID/PWM integration or EDS implementation.
- No complete upstream cFE/app coverage-suite run; the three focused tests above
  cover the new behavior. Updated stubs preserve linkage for existing app tests.
- Full sample mission (all external apps) was not rebuilt; the small mission was.

The package provides a working light-data producer and ADCS receiver, not a
finished satellite attitude controller or a validated sun sensor.
