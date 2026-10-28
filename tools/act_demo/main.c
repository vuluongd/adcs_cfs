/*
 * act_demo - run the ACT_APP motor layer without cFS.
 *
 *   ./act_demo                     sim: step to +150 rad/s, reverse to -100, stop; prints 10 Hz log
 *   ./act_demo 80                  sim: step to 80 rad/s and hold 8 s
 *   ACT_APP_BACKEND=gpio ./act_demo 50    on the Pi: same, driving the real pins (bench test!)
 */
#define _POSIX_C_SOURCE 200809L
#include "act_motor.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void tick(ACT_Motor_t *m, float setpoint, int ticks, int realtime)
{
    struct timespec ts = {0, 100000000L};
    ACT_MotorSetTarget(m, setpoint);
    for (int i = 0; i < ticks; i++)
    {
        uint8_t st = ACT_MotorStep(m, 0.1f);
        printf("t=%5.1f cmd=%7.1f tgt=%7.1f meas=%7.1f pfm=%7.0f dir=%+d en=%d brk=%d sat=%d status=%u sim=%u\n",
               i * 0.1, (double)m->st.cmd_omega, (double)m->st.target_omega, (double)m->st.meas_omega,
               (double)m->st.pfm_hz, m->st.direction, m->st.enabled, m->st.braking, m->st.saturated, st,
               m->st.simulated);
        if (realtime) nanosleep(&ts, NULL);
    }
}

int main(int argc, char **argv)
{
    ACT_Motor_t m;
    ACT_MotorInit(&m, NULL);
    int realtime = m.hardware; /* only pace in real time when driving hardware */

    if (argc == 2)
    {
        tick(&m, strtof(argv[1], NULL), 80, realtime);
    }
    else
    {
        puts("--- step +150 rad/s"); tick(&m, 150.0f, 50, realtime);
        puts("--- reverse to -100 rad/s (must pass through zero)"); tick(&m, -100.0f, 80, realtime);
        puts("--- stop"); ACT_MotorStop(&m); tick(&m, 0.0f, 40, realtime);
    }
    ACT_MotorClose(&m);
    return 0;
}
