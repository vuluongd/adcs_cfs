/* DU LIEU AO: lux + goc that trong MO HINH GIA de thu LIGHT_APP/ADCS_APP.
 * Chu ky 80 mau = 16 giay tai wakeup 5Hz, goc +/-30 do.
 * Bang duoi day KHONG PHẢI bang hieu chuan cua BH1750 that.
 */
#include "light_tracking.h"
#include <math.h>

static const LIGHT_CalPoint_t SimTable[] = {
    {-0.6f, -0.5235987756f},
    { 0.0f,  0.0f},
    { 0.6f,  0.5235987756f}
};

void LIGHT_UseSimulationCalibration(LIGHT_Config_t *cfg)
{
    if (cfg == NULL) return;
    cfg->calibration = SimTable;
    cfg->calibration_count = sizeof(SimTable) / sizeof(SimTable[0]);
}

uint8_t LIGHT_SimGenerate(uint32_t step, LIGHT_SimMode_t mode,
                          float *left, float *right, float *true_angle_rad)
{
    float wave;
    if (true_angle_rad != NULL) *true_angle_rad = NAN;
    if (left == NULL || right == NULL) return LIGHT_STATUS_BAD_INPUT;
    *left = *right = 0.0f;
    switch (mode)
    {
        case LIGHT_SIM_SWEEP:
            wave = sinf((float)(step % 80u) * (6.2831853072f / 80.0f));
            *left = 700.0f * (1.0f - 0.6f * wave);
            *right = 700.0f * (1.0f + 0.6f * wave);
            if (true_angle_rad != NULL) *true_angle_rad = 0.5235987756f * wave;
            return LIGHT_STATUS_OK;
        case LIGHT_SIM_DARK:
            return LIGHT_STATUS_OK; /* Estimator detects darkness from lux=0. */
        case LIGHT_SIM_IO_ERROR:
            return LIGHT_STATUS_IO_ERROR;
        case LIGHT_SIM_SATURATED:
            *left = *right = 65535.0f / 1.2f;
            return LIGHT_STATUS_SATURATED;
        default:
            return LIGHT_STATUS_BAD_INPUT;
    }
}
