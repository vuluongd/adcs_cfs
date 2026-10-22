#ifndef IMU_APP_PROCESSING_H
#define IMU_APP_PROCESSING_H

#include "cfe.h"

/* C entry points called by the cFS glue (imu_app.c, imu_app_dispatch.c, imu_app_cmds.c).
 * Implemented in C++ (imu_app_processing.cpp). */
#ifdef __cplusplus
extern "C"
{
#endif

CFE_Status_t IMU_APP_ProcessingInit(void);
void         IMU_APP_OnWakeup(void);
void         IMU_APP_ProcessingClose(void);
void         IMU_APP_Recalibrate(void);
void         IMU_APP_ResetAngle(void);

#ifdef __cplusplus
}
#endif

#endif
