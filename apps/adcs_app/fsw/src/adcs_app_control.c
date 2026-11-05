#include "adcs_app_control.h"
#include <math.h>
#include <string.h>

void ADCS_Ctrl_DefaultParams(ADCS_Ctrl_Params_t *p)
{
    p->kp                = ADCS_CTRL_DEFAULT_KP;
    p->omega_limit_rad_s = ADCS_CTRL_DEFAULT_LIMIT;
    p->deadband_rad      = ADCS_CTRL_DEFAULT_DEADBAND;
    p->hold_s            = ADCS_CTRL_DEFAULT_HOLD_S;
    p->wheel_sign        = ADCS_CTRL_DEFAULT_WHEEL_SIGN;
    p->imu_right_sign    = ADCS_CTRL_DEFAULT_IMU_RIGHT_SIGN;
}

bool ADCS_Ctrl_ParamsValid(const ADCS_Ctrl_Params_t *p)
{
    return isfinite(p->kp) && p->kp >= 0.0f && isfinite(p->omega_limit_rad_s) && p->omega_limit_rad_s > 0.0f &&
           p->omega_limit_rad_s <= ADCS_CTRL_ABS_MAX_LIMIT && isfinite(p->deadband_rad) && p->deadband_rad >= 0.0f &&
           p->deadband_rad < 1.5f && isfinite(p->hold_s) && p->hold_s >= 0.0f && fabsf(p->wheel_sign) == 1.0f &&
           fabsf(p->imu_right_sign) == 1.0f;
}

void ADCS_Ctrl_Reset(ADCS_Ctrl_Mem_t *m)
{
    memset(m, 0, sizeof(*m));
}

/* Trừ vùng chết: liên tục tại biên nên không bị giật lệnh. */
static float Deadband(float e, float db)
{
    if (e > db)  return e - db;
    if (e < -db) return e + db;
    return 0.0f;
}

static float Clamp(float v, float lim)
{
    if (v > lim)  return lim;
    if (v < -lim) return -lim;
    return v;
}

void ADCS_Ctrl_Step(const ADCS_Ctrl_Params_t *p, ADCS_Ctrl_Mem_t *m, const ADCS_Ctrl_In_t *in, float dt_s,
                    ADCS_Ctrl_Out_t *out)
{
    float err = 0.0f;
    bool  have_err = false;
    bool  fresh = in->light_usable && (!m->have_target || in->light_counter != m->last_light_counter);

    out->state = ADCS_CTRL_LOST;

    if (fresh)
    {
        /* Mẫu ánh sáng mới: ghi lại bearing cùng IMU angle tại thời điểm đó. */
        m->have_target        = true;
        m->bearing_cap_rad    = in->bearing_rad;
        m->last_light_counter = in->light_counter;
        m->since_light_s      = 0.0f;
        m->imu_cap_valid      = in->imu_usable;
        m->imu_cap_rad        = in->imu_usable ? in->imu_angle_rad : 0.0f;
        err      = in->bearing_rad;
        have_err = true;
        out->state = ADCS_CTRL_LIGHT;
    }
    else if (m->have_target)
    {
        m->since_light_s += dt_s;
        if (m->since_light_s <= p->hold_s && m->imu_cap_valid && in->imu_usable)
        {
            /* Thân đã quay (IMU) kể từ lúc đo ánh sáng => trừ phần đã quay khỏi bearing.
               Ví dụ: bearing = +0.5 (phải), thân quay phải 0.2 => imu đổi 0.2*imu_right_sign
               => err = 0.5 - 0.2 = 0.3. */
            err = m->bearing_cap_rad - p->imu_right_sign * (in->imu_angle_rad - m->imu_cap_rad);
            have_err   = true;
            out->state = ADCS_CTRL_IMU_HOLD;
        }
        else
        {
            /* Quá hạn giữ, hoặc không có IMU để ngoại suy: bearing cũ không còn đúng
               khi thân đang quay => dừng, KHÔNG dùng số liệu cũ. */
            m->have_target = false;
        }
    }

    if (!have_err)
    {
        out->error_rad       = 0.0f;
        out->omega_cmd_rad_s = 0.0f;
        return;
    }

    out->error_rad       = err;
    out->omega_cmd_rad_s = Clamp(p->wheel_sign * p->kp * Deadband(err, p->deadband_rad), p->omega_limit_rad_s);
}
