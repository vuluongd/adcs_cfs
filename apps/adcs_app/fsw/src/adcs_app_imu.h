#ifndef ADCS_APP_IMU_H
#define ADCS_APP_IMU_H
#include "cfe.h"

/* IMU_DATA is published at the IMU_APP wakeup rate (10 Hz = 100 ms). Three periods of
   margin covers Software Bus queue delay; older data is never used by the control loop. */
#define ADCS_APP_IMU_MAX_AGE_US 300000u

/* Called from the dispatcher for every IMU_APP_DATA_MID message. Validates the size,
   copies the payload into ADCS_APP_Data.Imu and stores the publication time. */
void ADCS_APP_ReceiveImu(const CFE_SB_Buffer_t *buffer);

/* Call at EVERY control tick. true only if the newest IMU_DATA is fresh and trustworthy:
   bias calibrated (valid=1, status OK), finite numbers, rate inside the gyro range.
   false => do not use the old angle / rate in the estimator or PID. */
bool ADCS_APP_ImuUsable(bool allow_simulated);

#endif
