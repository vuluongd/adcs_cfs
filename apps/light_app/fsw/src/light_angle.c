/* XAC DINH GOC: noi suy bang hieu chuan cua dung cum BH1750 + vach chan.
 * Chieu duong ve cam bien 2/RIGHT. Dau vao error khong co don vi; dau ra radian.
 * Khong dung atan(error), khong tu gan 0.2 thanh 20 do.
 */
#include "light_tracking.h"
#include <math.h>

uint8_t LIGHT_AngleFromError(const LIGHT_Config_t *cfg, float error, float *angle_rad)
{
    if (angle_rad == NULL) return LIGHT_STATUS_BAD_INPUT;
    *angle_rad = 0.0f; /* Placeholder; check return status before using this value. */
    if (cfg == NULL || !isfinite(error) || fabsf(error) > 1.0f)
        return LIGHT_STATUS_BAD_INPUT;
    if (cfg->calibration == NULL && cfg->calibration_count == 0)
        return LIGHT_STATUS_NO_CALIBRATION;
    if (cfg->calibration == NULL || cfg->calibration_count < 2 || cfg->calibration_count > 32)
        return LIGHT_STATUS_BAD_INPUT;

    for (size_t i = 0; i < cfg->calibration_count; ++i)
    {
        const LIGHT_CalPoint_t *p = &cfg->calibration[i];
        if (!isfinite(p->normalized_diff) || fabsf(p->normalized_diff) > 1.0f ||
            !isfinite(p->bearing_rad) || fabsf(p->bearing_rad) >= 1.570796327f)
            return LIGHT_STATUS_BAD_INPUT;
        if (i > 0 &&
            (p->normalized_diff - cfg->calibration[i-1].normalized_diff < 0.0001f ||
             p->bearing_rad <= cfg->calibration[i-1].bearing_rad))
            return LIGHT_STATUS_BAD_INPUT;
    }

    if (error < cfg->calibration[0].normalized_diff ||
        error > cfg->calibration[cfg->calibration_count-1].normalized_diff)
        return LIGHT_STATUS_OUT_OF_VIEW; /* No extrapolation. */

    for (size_t i = 1; i < cfg->calibration_count; ++i)
    {
        const LIGHT_CalPoint_t *a = &cfg->calibration[i-1];
        const LIGHT_CalPoint_t *b = &cfg->calibration[i];
        if (error <= b->normalized_diff)
        {
            float weight = (error - a->normalized_diff) /
                           (b->normalized_diff - a->normalized_diff);
            *angle_rad = a->bearing_rad + weight * (b->bearing_rad - a->bearing_rad);
            return LIGHT_STATUS_OK;
        }
    }
    return LIGHT_STATUS_BAD_INPUT;
}
