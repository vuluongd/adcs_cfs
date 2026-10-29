/* Unit test for the cFS-free motor layer (sim backend). */
#include "act_motor.h"
#undef NDEBUG
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void run(ACT_Motor_t *m, int ticks) { for (int i = 0; i < ticks; i++) ACT_MotorStep(m, 0.1f); }

int main(void)
{
    ACT_Motor_t m;
    ACT_MotorConfig_t c;
    ACT_MotorDefaultConfig(&c);
    ACT_MotorInit(&m, &c);
    assert(m.st.simulated == 1 && m.st.status == ACT_STATUS_NOT_READY);

    /* bad input */
    assert(ACT_MotorSetTarget(&m, NAN) == ACT_STATUS_BAD_INPUT);

    /* clamp */
    assert(ACT_MotorSetTarget(&m, 10.0f * c.omega_max_rad_s) == ACT_STATUS_SATURATED);
    assert(m.st.cmd_omega == c.omega_max_rad_s && m.st.saturated);

    /* slew: after 1 s target must be <= slew*1s (+epsilon) */
    ACT_MotorSetTarget(&m, 100.0f);
    run(&m, 10);
    assert(m.st.target_omega <= c.slew_rad_s2 * 1.0f + 1e-3f);
    assert(m.st.enabled == 1 && m.st.braking == 0 && m.st.direction == 1);
    assert(m.st.pfm_hz >= c.pfm_min_hz && m.st.pfm_hz <= c.pfm_max_hz);

    /* settle */
    run(&m, 100);
    assert(fabsf(m.st.target_omega - 100.0f) < 1e-3f);
    assert(fabsf(m.st.meas_omega - 100.0f) < 5.0f);
    assert(m.st.status == ACT_STATUS_OK);

    /* reverse: direction must not flip while the wheel is still fast */
    ACT_MotorSetTarget(&m, -100.0f);
    run(&m, 5);
    assert(m.st.direction == 1);
    assert(m.st.target_omega > 0.0f && m.st.target_omega < 100.0f);
    run(&m, 200);
    assert(m.st.direction == -1);
    assert(fabsf(m.st.meas_omega + 100.0f) < 5.0f);

    /* stop: brake + disable + pfm 0 */
    ACT_MotorStop(&m);
    run(&m, 200);
    assert(m.st.enabled == 0 && m.st.braking == 1 && m.st.pfm_hz == 0.0f);
    assert(fabsf(m.st.meas_omega) < c.stop_threshold);

    ACT_MotorClose(&m);
    puts("test_motor: all checks passed");
    return 0;
}
