/*
 * act_app_wheel.h - cFS glue between ACT_APP and the motor layer (act_motor.h).
 */
#ifndef ACT_APP_WHEEL_H
#define ACT_APP_WHEEL_H

#include "cfe.h"

CFE_Status_t ACT_APP_WheelInit(void);            /* subscribe WAKEUP, init WHEEL_DATA header, init motor */
void         ACT_APP_OnWakeup(void);             /* step motor, publish WHEEL_DATA                        */
void         ACT_APP_WheelClose(void);           /* brake + release hardware                              */
uint8        ACT_APP_WheelSetSpeed(float omega); /* from ACT_CMD; returns ACT_STATUS_*                    */
void         ACT_APP_WheelStop(void);

#endif /* ACT_APP_WHEEL_H */
