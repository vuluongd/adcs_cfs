/*
 * act_motor.h - reaction-wheel motor layer for ACT_APP.  No cFS dependency, so it
 * can be built and tested on a PC (tools/act_demo) like light_tracking_core.
 *
 * Target hardware: Nidec 24H055M020 BLDC with integrated driver, controlled by
 *   ENABLE (high = run), BRAKE (low = brake), DIR (high = CCW), and a PFM speed
 *   input where the pulse FREQUENCY (not duty) sets speed, ~150 Hz .. ~25 kHz.
 *   Speed feedback (FG) may or may not be exposed; the layer works without it.
 *
 * Backends (env ACT_APP_BACKEND):
 *   unset / "sim" : first-order model of the wheel, simulated = 1
 *   "gpio"        : Raspberry Pi hardware PWM (variable period, 50 % duty) + GPIOs
 *   anything else : BAD_INPUT, never silently simulates (same rule as IMU/LIGHT)
 */
#ifndef ACT_MOTOR_H
#define ACT_MOTOR_H

#include <stdint.h>
#include "act_status.h"

typedef struct
{
    float omega_max_rad_s;   /* hard clamp on |setpoint|                               */
    float slew_rad_s2;       /* max |d(target)/dt|: the 24H055 dislikes step changes   */
    float pfm_min_hz;        /* below this the Nidec does not start                    */
    float pfm_max_hz;        /* above this it self-shuts; stay well under 15 kHz       */
    float rad_s_per_hz;      /* open-loop map omega = rad_s_per_hz * pfm_hz (bench)    */
    float sim_tau_s;         /* simulated wheel time constant                          */
    float stop_threshold;    /* |target| below this => brake and drop ENABLE           */
} ACT_MotorConfig_t;

typedef struct
{
    float   cmd_omega;       /* last setpoint requested                                */
    float   target_omega;    /* after clamp + slew                                     */
    float   meas_omega;      /* measured (FG) or modelled                              */
    float   pfm_hz;          /* frequency currently commanded                          */
    uint8_t status;          /* ACT_STATUS_*                                           */
    uint8_t saturated;
    uint8_t simulated;
    uint8_t enabled;
    uint8_t braking;
    int8_t  direction;       /* +1 CCW, -1 CW, 0 stopped                               */
    uint8_t measured;        /* 1 = meas_omega came from FG                            */
} ACT_MotorState_t;

typedef struct
{
    ACT_MotorConfig_t cfg;
    ACT_MotorState_t  st;
    int               hardware;      /* 0 = sim, 1 = gpio                              */
    int               backend_valid; /* 0 = unknown backend string                     */
    /* gpio backend handles (unused in sim) */
    int               pwm_fd_period;
    int               pwm_fd_duty;
    int               gpio_ready;
    /* sim backend state */
    float             sim_omega;
} ACT_Motor_t;

void    ACT_MotorDefaultConfig(ACT_MotorConfig_t *cfg);
void    ACT_MotorInit(ACT_Motor_t *m, const ACT_MotorConfig_t *cfg); /* reads ACT_APP_BACKEND */
void    ACT_MotorClose(ACT_Motor_t *m);                              /* brake, disable, release */
uint8_t ACT_MotorSetTarget(ACT_Motor_t *m, float omega_rad_s);       /* ACT_STATUS_* */
void    ACT_MotorStop(ACT_Motor_t *m);                               /* setpoint 0, brake when slow */
uint8_t ACT_MotorStep(ACT_Motor_t *m, float dt_s);                   /* call every wakeup */

#endif /* ACT_MOTOR_H */
