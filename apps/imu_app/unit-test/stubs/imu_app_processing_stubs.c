/*
 * Stubs for the C entry points implemented in fsw/src/imu_app_processing.cpp,
 * so the C coverage tests of imu_app.c / dispatch / cmds link without the C++ core.
 */
#include "cfe.h"
#include "imu_app_processing.h"

CFE_Status_t IMU_APP_ProcessingInit(void) { return CFE_SUCCESS; }
void         IMU_APP_OnWakeup(void) {}
void         IMU_APP_ProcessingClose(void) {}
void         IMU_APP_Recalibrate(void) {}
void         IMU_APP_ResetAngle(void) {}
