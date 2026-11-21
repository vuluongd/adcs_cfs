#ifndef ACT_MOTOR_H
#define ACT_MOTOR_H

#include <stdint.h>
#include "act_status.h"

typedef struct
{
    float omega_max_rad_s;   /* hard clamp on |setpoint|                               */
    float slew_rad_s2;       /* max |d(target)/dt|: the 24H055 dislikes step changes   */
    float pwm_freq_hz;       /* FIXED PWM carrier frequency (speed = duty, not frequency) */
    float duty_min_pct;      /* below this the motor does not start (dead zone)        */
    float duty_max_pct;      /* upper clamp, <= 100                                    */
    float rad_s_per_duty;    /* open-loop map omega = rad_s_per_duty * duty_pct (bench)*/
    float sim_tau_s;         /* simulated wheel time constant                          */
    float stop_threshold;    /* |target| below this => brake and drop ENABLE           */
    uint8_t use_pi;          /* 0 = open-loop, 1 = PI + Feedforward */
    float   kp;                 /* % duty per rad/s of error   */
    float   ki;                 /* % duty per rad (integrated) */
    float   integral_max;       /* anti-windup clamp, % duty   */
    uint8_t use_encoder;        /* 1 = đọc encoder thật        */
    float   enc_counts_per_rev; /* 100 (x1) hoặc 400 (x4)      */
    float   enc_lpf_alpha;      /* 0.2..0.5, lọc tốc độ        */
} ACT_MotorConfig_t;

typedef struct
{
    float   cmd_omega;       /* last setpoint requested                                */
    float   target_omega;    /* after clamp + slew                                     */
    float   meas_omega;      /* measured (FG) or modelled                              */
    float   duty_pct;        /* PWM duty (0..100 %) currently commanded                */
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
    unsigned long     pwm_period_ns; /* fixed period = 1e9 / cfg.pwm_freq_hz           */
    /* sim backend state */
    float             sim_omega;
    float             integral;
    /* encoder (UART từ RL78) */
    int               enc_fd;
    uint32_t          enc_last_count;
    uint8_t           enc_have_last;
    uint8_t           enc_stale;     /* số chu kỳ liên tiếp không có frame mới */
    float             enc_dt_acc;    /* thời gian cộng dồn kể từ frame trước   */
    float             omega_filt;
} ACT_Motor_t;

void    ACT_MotorDefaultConfig(ACT_MotorConfig_t *cfg);
void    ACT_MotorInit(ACT_Motor_t *m, const ACT_MotorConfig_t *cfg); /* reads ACT_APP_BACKEND */
void    ACT_MotorClose(ACT_Motor_t *m);                              /* brake, disable, release */
uint8_t ACT_MotorSetTarget(ACT_Motor_t *m, float omega_rad_s);       /* ACT_STATUS_* */
void    ACT_MotorStop(ACT_Motor_t *m);                               /* setpoint 0, brake when slow */
uint8_t ACT_MotorStep(ACT_Motor_t *m, float dt_s);                   /* call every wakeup */
uint8_t ACT_MotorSetMode(ACT_Motor_t *m, uint8_t use_pi);            /* 0 = open-loop, 1 = PI */

#endif /* ACT_MOTOR_H */
