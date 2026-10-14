#include "light_tracking.h"
#include <math.h>
#include <string.h>

void LIGHT_DefaultConfig(LIGHT_Config_t *cfg)
{
    *cfg = (LIGHT_Config_t){
        .left_gain=1.0f, .right_gain=1.0f,
        .left_offset_lux=0.0f, .right_offset_lux=0.0f,
        .min_sum_lux=20.0f, .filter_alpha=0.5f,
        .calibration=NULL, .calibration_count=0
    };
}

void LIGHT_ResetFilter(LIGHT_Filter_t *filter)
{
    memset(filter, 0, sizeof(*filter));
}

static int LIGHT_ConfigValid(const LIGHT_Config_t *c)
{
    return isfinite(c->left_gain) && c->left_gain > 0.0f &&
           isfinite(c->right_gain) && c->right_gain > 0.0f &&
           isfinite(c->left_offset_lux) && c->left_offset_lux >= 0.0f &&
           isfinite(c->right_offset_lux) && c->right_offset_lux >= 0.0f &&
           isfinite(c->min_sum_lux) && c->min_sum_lux > 0.0f &&
           isfinite(c->filter_alpha) && c->filter_alpha > 0.0f && c->filter_alpha <= 1.0f &&
           ((c->calibration == NULL && c->calibration_count == 0) ||
            (c->calibration != NULL && c->calibration_count >= 2 && c->calibration_count <= 32));
}

void LIGHT_Estimate(const LIGHT_Config_t *cfg, LIGHT_Filter_t *filter,
                    float left_lux, float right_lux, uint8_t input_status,
                    LIGHT_DataPayload_t *out)
{
    float left, right, sum, current_diff, current_angle;
    uint8_t angle_status;
    memset(out, 0, sizeof(*out));
    out->status=input_status;
    if (input_status!=LIGHT_STATUS_OK)
    {
        LIGHT_ResetFilter(filter);
        return;
    }
    if (!LIGHT_ConfigValid(cfg) || !isfinite(left_lux) || !isfinite(right_lux) ||
        left_lux<0.0f || right_lux<0.0f)
    {
        out->status=LIGHT_STATUS_BAD_INPUT;
        LIGHT_ResetFilter(filter);
        return;
    }
    left=fmaxf(0.0f,left_lux-cfg->left_offset_lux)*cfg->left_gain;
    right=fmaxf(0.0f,right_lux-cfg->right_offset_lux)*cfg->right_gain;
    sum=left+right;
    if (!isfinite(sum))
    {
        out->status=LIGHT_STATUS_BAD_INPUT;
        LIGHT_ResetFilter(filter);
        return;
    }
    out->left_lux=left;
    out->right_lux=right;
    if (sum<cfg->min_sum_lux)
    {
        out->status=LIGHT_STATUS_DARK;
        LIGHT_ResetFilter(filter);
        return;
    }
    current_diff=(right-left)/sum;
    angle_status=LIGHT_AngleFromError(cfg,current_diff,&current_angle);
    if (angle_status==LIGHT_STATUS_BAD_INPUT)
    {
        out->status=angle_status;
        LIGHT_ResetFilter(filter);
        return;
    }
    /* Check CURRENT sample before smoothing; preserve direction outside LUT. */
    if (angle_status==LIGHT_STATUS_OUT_OF_VIEW)
    {
        out->normalized_diff=current_diff;
        out->error_valid=1;
        out->status=angle_status;
        LIGHT_ResetFilter(filter);
        return;
    }
    if (!filter->initialized)
    {
        filter->left_filtered=left;
        filter->right_filtered=right;
        filter->initialized=1;
    }
    else
    {
        filter->left_filtered+=cfg->filter_alpha*(left-filter->left_filtered);
        filter->right_filtered+=cfg->filter_alpha*(right-filter->right_filtered);
    }
    out->normalized_diff=(filter->right_filtered-filter->left_filtered)/
                         (filter->right_filtered+filter->left_filtered);
    out->error_valid=1;
    out->status=LIGHT_AngleFromError(cfg,out->normalized_diff,&out->bearing_rad);
    out->valid=(out->status==LIGHT_STATUS_OK) ? 1 : 0;
    if (out->status==LIGHT_STATUS_OUT_OF_VIEW) LIGHT_ResetFilter(filter);
}
