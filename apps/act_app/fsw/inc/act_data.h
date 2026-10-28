/*
 * act_data.h - WHEEL_DATA packet published by ACT_APP (TLM MID 0x08E4, 10 Hz)
 *              and the WAKEUP_ACT command it receives from SCH_LAB.
 *
 * This is the interface header: ADCS_APP includes it the same way it includes
 * light_data.h / imu_data.h.  Offsets below are relative to the start of the
 * payload, i.e. 12 bytes after the start of the packet (CCSDS telemetry header).
 */
#ifndef ACT_DATA_H
#define ACT_DATA_H

#include "cfe.h"
#include "act_status.h"

typedef struct
{
    float  cmd_omega_rad_s;    /* off 0  last setpoint received (ACT_CMD), before clamping   */
    float  target_omega_rad_s; /* off 4  setpoint after clamp + slew limiter (what we drive) */
    float  meas_omega_rad_s;   /* off 8  wheel speed: FG-measured if available, else model   */
    float  pfm_hz;             /* off 12 pulse frequency currently sent to the Nidec driver  */
    uint32 sample_counter;     /* off 16 increments every wakeup, even on error              */
    uint8  valid;              /* off 20 1 = meas_omega is usable                            */
    uint8  status;             /* off 21 ACT_STATUS_* (0 = OK)                               */
    uint8  saturated;          /* off 22 1 = setpoint was clamped to omega_max               */
    uint8  simulated;          /* off 23 1 = simulation backend, ADCS must not trust this    */
    uint8  enabled;            /* off 24 1 = motor ENABLE line asserted                      */
    uint8  braking;            /* off 25 1 = BRAKE line asserted                             */
    int8   direction;          /* off 26 +1 CCW, -1 CW, 0 stopped (driver DIR convention)    */
    uint8  measured;           /* off 27 1 = meas_omega from FG pulses, 0 = open-loop model  */
} ACT_WheelDataPayload_t;      /* 28 bytes */

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    ACT_WheelDataPayload_t    Payload;
} ACT_WheelDataTlm_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} ACT_WakeupCmd_t;

#endif /* ACT_DATA_H */
