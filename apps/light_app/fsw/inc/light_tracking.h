#ifndef LIGHT_TRACKING_H
#define LIGHT_TRACKING_H

#include <stdint.h>
#include <stddef.h>

/* Body-frame bearing: positive toward RIGHT. Radians, not absolute attitude. */
enum
{
    LIGHT_STATUS_OK = 0,
    LIGHT_STATUS_DARK = 1,
    LIGHT_STATUS_SATURATED = 2,
    LIGHT_STATUS_OUT_OF_VIEW = 3,
    LIGHT_STATUS_IO_ERROR = 4,
    LIGHT_STATUS_NOT_READY = 5,
    LIGHT_STATUS_BAD_INPUT = 6,
    LIGHT_STATUS_NO_CALIBRATION = 7
};

typedef struct
{
    float normalized_diff; /* Measured (R-L)/(R+L) at a known angle */
    float bearing_rad;
} LIGHT_CalPoint_t;

typedef struct
{
    float left_gain;
    float right_gain;
    float left_offset_lux;
    float right_offset_lux;
    float min_sum_lux;
    float filter_alpha;        /* EMA: 1 = no smoothing */
    const LIGHT_CalPoint_t *calibration; /* NULL until measured on THIS baffle */
    size_t calibration_count;           /* 2..32 strictly increasing points */
} LIGHT_Config_t;

typedef struct
{
    float left_filtered;
    float right_filtered;
    uint8_t initialized;
} LIGHT_Filter_t;

/* Shared LIGHT_DATA payload; fixed-width fields, no bool or enum on the wire. */
typedef struct
{
    float left_lux;            /* Corrected current readings, before filtering */
    float right_lux;
    float normalized_diff;     /* Filtered (R-L)/(R+L) */
    float bearing_rad;         /* Meaningful only when valid == 1 */
    uint32_t sample_counter;   /* Increments even for invalid measurements */
    uint8_t valid;             /* ANGLE validity; 0 until calibration exists */
    uint8_t status;
    uint8_t simulated;         /* ADCS must distinguish real data from demo */
    uint8_t error_valid;       /* Validity of dimensionless normalized_diff */
} LIGHT_DataPayload_t;

void LIGHT_DefaultConfig(LIGHT_Config_t *cfg);
void LIGHT_ResetFilter(LIGHT_Filter_t *filter);
void LIGHT_Estimate(const LIGHT_Config_t *cfg, LIGHT_Filter_t *filter,
                    float left_lux, float right_lux, uint8_t input_status,
                    LIGHT_DataPayload_t *out);

/* light_angle.c: no angle output without a valid measured calibration table. */
uint8_t LIGHT_AngleFromError(const LIGHT_Config_t *cfg, float error, float *angle_rad);

/* light_sim.c: synthetic model ONLY; never attach this table to real sensors. */
typedef enum
{
    LIGHT_SIM_SWEEP = 0,
    LIGHT_SIM_DARK,
    LIGHT_SIM_IO_ERROR,
    LIGHT_SIM_SATURATED
} LIGHT_SimMode_t;
uint8_t LIGHT_SimGenerate(uint32_t step, LIGHT_SimMode_t mode,
                          float *left, float *right, float *true_angle_rad);
void LIGHT_UseSimulationCalibration(LIGHT_Config_t *cfg);

#endif
