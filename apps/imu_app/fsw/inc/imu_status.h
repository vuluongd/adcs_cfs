#ifndef IMU_STATUS_H
#define IMU_STATUS_H

/* Plain C so both the C cFS glue and the C++ core can share it.
 * Value of IMU_DATA payload field "status". 0 = OK (same convention as LIGHT_DATA). */
enum
{
    IMU_STATUS_OK          = 0,
    IMU_STATUS_CALIBRATING = 1,
    IMU_STATUS_NOT_READY   = 2,
    IMU_STATUS_IO_ERROR    = 3,
    IMU_STATUS_BAD_ID      = 4,
    IMU_STATUS_SATURATED   = 5,
    IMU_STATUS_BAD_INPUT   = 6
};

#endif
