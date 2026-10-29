/*
 * act_status.h - status codes shared by the ACT_APP motor layer, the cFS glue
 * and ADCS_APP. Same convention as imu_status.h / light_tracking.h: 0 = OK.
 */
#ifndef ACT_STATUS_H
#define ACT_STATUS_H

enum
{
    ACT_STATUS_OK        = 0, /* tracking the setpoint, speed value usable        */
    ACT_STATUS_NOT_READY = 1, /* backend initialising / motor not yet enabled    */
    ACT_STATUS_IO_ERROR  = 2, /* GPIO/PWM open or write failed                    */
    ACT_STATUS_SATURATED = 3, /* setpoint clamped to +/- omega_max                */
    ACT_STATUS_FAULT     = 4, /* motor stopped on its own (24H055 self-shutdown)  */
    ACT_STATUS_BAD_INPUT = 5  /* non-finite setpoint, unknown backend, bad config */
};

#endif /* ACT_STATUS_H */
