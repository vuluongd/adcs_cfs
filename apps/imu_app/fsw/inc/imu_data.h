#ifndef IMU_DATA_H
#define IMU_DATA_H

#include "cfe.h"
#include "imu_status.h"

/*
 * IMU_DATA payload, 32 bytes, all fields naturally aligned (offsets after the 12-byte TLM header).
 * SI units (rad) like LIGHT_DATA.bearing_rad so ADCS_APP can fuse both directly.
 */
typedef struct
{
    float    raw_rate_rad_s;  /* off 0  gyro Z as read, bias NOT removed                    */
    float    rate_rad_s;      /* off 4  median + low-pass filtered, bias removed            */
    float    angle_rad;       /* off 8  integrated angle since last RESET_ANGLE / boot      */
    float    bias_rad_s;      /* off 12 current bias estimate                               */
    float    temp_c;          /* off 16 MPU6050 die temperature                             */
    float    dt_s;            /* off 20 real time since the previous accepted sample        */
    uint32   sample_counter;  /* off 24                                                     */
    uint8    valid;           /* off 28 1 = calibrated and sample accepted                  */
    uint8    status;          /* off 29 IMU_STATUS_* (0 = OK)                               */
    uint8    stationary;      /* off 30 1 = still: angle frozen, bias adapting              */
    uint8    simulated;       /* off 31 1 = simulation backend                              */
} IMU_DataPayload_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    IMU_DataPayload_t         Payload;
} IMU_DataTlm_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} IMU_WakeupCmd_t;

#endif
