/*
 * act_motor.c - reaction-wheel motor layer (see act_motor.h).
 *
 * The control logic (clamp, slew, direction reversal, brake) is identical for both
 * backends; only "write the output" and "read the speed" differ.  Nothing in here
 * sleeps or blocks: the cFS wakeup handler calls ACT_MotorStep() at 10 Hz.
 */
#define _POSIX_C_SOURCE 200809L
#include "act_motor.h"

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ------------------------------------------------------------------------- */
/* Defaults from the Nidec 24H055M020 bench test (project slides 38 / 61):     */
/*   - speed is set by pulse FREQUENCY, 0.15 rpm per Hz, linear up to 10 kHz   */
/*     (~1500 rpm); above 10 kHz the curve flattens, so we never go there.     */
/*   - driver accepts 100 Hz .. 25 kHz; it starts at 100 Hz.                   */
/* 0.15 rpm/Hz * 2*pi/60 = 0.015708 rad/s per Hz;  10 kHz -> 157 rad/s.        */
/* ------------------------------------------------------------------------- */
void ACT_MotorDefaultConfig(ACT_MotorConfig_t *cfg)
{
    *cfg = (ACT_MotorConfig_t){
        .omega_max_rad_s = 157.0f,    /* 10 kHz * 0.015708 = top of the linear region (~1500 rpm) */
        .slew_rad_s2     = 60.0f,     /* 0 -> 157 rad/s in ~2.6 s                                 */
        .pfm_min_hz      = 100.0f,    /* bench: motor starts at 100 Hz                            */
        .pfm_max_hz      = 10000.0f,  /* bench: linear only up to 10 kHz                          */
        .rad_s_per_hz    = 0.015708f, /* bench: 0.15 rpm/Hz                                       */
        .sim_tau_s       = 0.8f,
        .stop_threshold  = 2.0f       /* rad/s (~19 rpm)                                          */
    };
}

static int ACT_ConfigValid(const ACT_MotorConfig_t *c)
{
    return isfinite(c->omega_max_rad_s) && c->omega_max_rad_s > 0.0f &&
           isfinite(c->slew_rad_s2) && c->slew_rad_s2 > 0.0f &&
           isfinite(c->pfm_min_hz) && c->pfm_min_hz > 0.0f &&
           isfinite(c->pfm_max_hz) && c->pfm_max_hz > c->pfm_min_hz &&
           isfinite(c->rad_s_per_hz) && c->rad_s_per_hz > 0.0f &&
           isfinite(c->sim_tau_s) && c->sim_tau_s > 0.0f &&
           isfinite(c->stop_threshold) && c->stop_threshold >= 0.0f;
}

/* ------------------------------------------------------------------------- */
/* gpio backend: Raspberry Pi sysfs.                                           */
/* Wiring (project slides 40 / 57, Pi -> 74AHCT125 3.3->5 V -> Nidec 12-pin):  */
/*   GPIO18 hardware PWM -> pin 12 PFM      (dtoverlay=pwm, pwmchip0/pwm0)     */
/*   GPIO24              -> pin 11 Start    (ACT_APP_GPIO_ENABLE)               */
/*   GPIO23              -> pin 9  CW/CCW   (ACT_APP_GPIO_DIR)                  */
/*   GPIO25              -> pin 10 BRAKE    (ACT_APP_GPIO_BRAKE)                */
/*   pins 1-2 +12 V, 3-5 GND, 6 TOOL0, 7 RESET, 8 Status: never driven by us.  */
/* PFM = hardware PWM with variable period and 50 % duty.                      */
/* Env: ACT_APP_PWM_DIR, ACT_APP_GPIO_ENABLE/DIR/BRAKE (BCM numbers) and        */
/* ACT_APP_GPIO_BASE: sysfs number = BASE + BCM.  Pi kernels >= 6.6 put the     */
/* GPIO chip at base 512 (GPIO18 = gpio530); check /sys/class/gpio/gpiochip*. */
/* ------------------------------------------------------------------------- */
static const char *ACT_Env(const char *name, const char *dflt)
{
    const char *v = getenv(name);
    return (v != NULL && v[0] != '\0') ? v : dflt;
}

static int ACT_WriteStr(const char *path, const char *val)
{
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    ssize_t n;
    if (fd < 0) return -1;
    n = write(fd, val, strlen(val));
    close(fd);
    return (n == (ssize_t)strlen(val)) ? 0 : -1;
}

#define ACT_GPIO_ENABLE_DFLT "24" /* Nidec pin 11 Start  */
#define ACT_GPIO_DIR_DFLT    "23" /* Nidec pin 9  CW/CCW */
#define ACT_GPIO_BRAKE_DFLT  "25" /* Nidec pin 10 BRAKE  */

/* sysfs GPIO number for a BCM pin: ACT_APP_GPIO_BASE (default 0) + BCM */
static long ACT_GpioNum(const char *bcm)
{
    return strtol(ACT_Env("ACT_APP_GPIO_BASE", "0"), NULL, 10) + strtol(bcm, NULL, 10);
}

static int ACT_GpioWrite(const char *bcm, int level)
{
    char path[96];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%ld/value", ACT_GpioNum(bcm));
    return ACT_WriteStr(path, level ? "1" : "0");
}

static int ACT_GpioExportOut(const char *bcm)
{
    char path[96], num[16];
    long n = ACT_GpioNum(bcm);
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%ld/direction", n);
    if (access(path, W_OK) != 0)
    {
        snprintf(num, sizeof(num), "%ld", n);
        (void)ACT_WriteStr("/sys/class/gpio/export", num); /* may already exist */
    }
    return ACT_WriteStr(path, "out");
}

static int ACT_GpioOpen(ACT_Motor_t *m)
{
    const char *pwmdir = ACT_Env("ACT_APP_PWM_DIR", "/sys/class/pwm/pwmchip0/pwm0");
    char path[160];

    if (ACT_GpioExportOut(ACT_Env("ACT_APP_GPIO_ENABLE", ACT_GPIO_ENABLE_DFLT)) != 0 ||
        ACT_GpioExportOut(ACT_Env("ACT_APP_GPIO_DIR", ACT_GPIO_DIR_DFLT)) != 0 ||
        ACT_GpioExportOut(ACT_Env("ACT_APP_GPIO_BRAKE", ACT_GPIO_BRAKE_DFLT)) != 0)
        return -1;

    snprintf(path, sizeof(path), "%s/period", pwmdir);
    m->pwm_fd_period = open(path, O_WRONLY | O_CLOEXEC);
    snprintf(path, sizeof(path), "%s/duty_cycle", pwmdir);
    m->pwm_fd_duty = open(path, O_WRONLY | O_CLOEXEC);
    if (m->pwm_fd_period < 0 || m->pwm_fd_duty < 0) return -1;

    snprintf(path, sizeof(path), "%s/enable", pwmdir);
    (void)ACT_WriteStr(path, "0");
    m->gpio_ready = 1;
    return 0;
}

static int ACT_GpioApply(ACT_Motor_t *m)
{
    const char *pwmdir = ACT_Env("ACT_APP_PWM_DIR", "/sys/class/pwm/pwmchip0/pwm0");
    char buf[32], path[160];
    ACT_MotorState_t *s = &m->st;

    if (ACT_GpioWrite(ACT_Env("ACT_APP_GPIO_BRAKE", ACT_GPIO_BRAKE_DFLT), !s->braking) != 0) return -1; /* pin 10: low = brake (TODO confirm on bench) */
    if (ACT_GpioWrite(ACT_Env("ACT_APP_GPIO_DIR", ACT_GPIO_DIR_DFLT), s->direction > 0) != 0) return -1; /* pin 9: high = CCW (TODO confirm on bench) */
    if (ACT_GpioWrite(ACT_Env("ACT_APP_GPIO_ENABLE", ACT_GPIO_ENABLE_DFLT), s->enabled) != 0) return -1; /* pin 11: high = run (TODO confirm on bench) */

    snprintf(path, sizeof(path), "%s/enable", pwmdir);
    if (s->pfm_hz > 0.0f)
    {
        unsigned long period_ns = (unsigned long)(1.0e9f / s->pfm_hz);
        /* order matters in sysfs: duty must stay <= period */
        if (dprintf(m->pwm_fd_duty, "0") < 0) return -1;
        snprintf(buf, sizeof(buf), "%lu", period_ns);
        if (dprintf(m->pwm_fd_period, "%s", buf) < 0) return -1;
        snprintf(buf, sizeof(buf), "%lu", period_ns / 2u);
        if (dprintf(m->pwm_fd_duty, "%s", buf) < 0) return -1;
        return ACT_WriteStr(path, "1");
    }
    return ACT_WriteStr(path, "0");
}

/* ------------------------------------------------------------------------- */
void ACT_MotorInit(ACT_Motor_t *m, const ACT_MotorConfig_t *cfg)
{
    const char *backend = getenv("ACT_APP_BACKEND");

    memset(m, 0, sizeof(*m));
    m->pwm_fd_period = m->pwm_fd_duty = -1;
    if (cfg != NULL) m->cfg = *cfg; else ACT_MotorDefaultConfig(&m->cfg);

    /* Unknown backend is treated as hardware so it fails loudly, never silently simulates. */
    m->hardware      = backend != NULL && strcmp(backend, "sim") != 0;
    m->backend_valid = !m->hardware || strcmp(backend, "gpio") == 0;

    m->st.simulated = m->hardware ? 0 : 1;
    m->st.status    = ACT_STATUS_NOT_READY;
    m->st.braking   = 1;
    m->st.enabled   = 0;
    m->st.direction = 0;
}

void ACT_MotorClose(ACT_Motor_t *m)
{
    m->st.cmd_omega = m->st.target_omega = 0.0f;
    m->st.pfm_hz    = 0.0f;
    m->st.enabled   = 0;
    m->st.braking   = 1;
    if (m->hardware && m->gpio_ready) (void)ACT_GpioApply(m);
    if (m->pwm_fd_period >= 0) close(m->pwm_fd_period);
    if (m->pwm_fd_duty >= 0) close(m->pwm_fd_duty);
    m->pwm_fd_period = m->pwm_fd_duty = -1;
    m->gpio_ready = 0;
}

uint8_t ACT_MotorSetTarget(ACT_Motor_t *m, float omega_rad_s)
{
    if (!isfinite(omega_rad_s)) return ACT_STATUS_BAD_INPUT;
    m->st.saturated = 0;
    if (omega_rad_s > m->cfg.omega_max_rad_s)       { omega_rad_s = m->cfg.omega_max_rad_s;  m->st.saturated = 1; }
    else if (omega_rad_s < -m->cfg.omega_max_rad_s) { omega_rad_s = -m->cfg.omega_max_rad_s; m->st.saturated = 1; }
    m->st.cmd_omega = omega_rad_s;
    return m->st.saturated ? ACT_STATUS_SATURATED : ACT_STATUS_OK;
}

void ACT_MotorStop(ACT_Motor_t *m)
{
    m->st.cmd_omega = 0.0f;
    m->st.saturated = 0;
}

static float ACT_SlewToward(float current, float goal, float max_step)
{
    float d = goal - current;
    if (d > max_step) d = max_step;
    if (d < -max_step) d = -max_step;
    return current + d;
}

uint8_t ACT_MotorStep(ACT_Motor_t *m, float dt_s)
{
    ACT_MotorState_t *s = &m->st;
    const ACT_MotorConfig_t *c = &m->cfg;
    float goal, max_step, drive_omega;
    int   desired_dir;

    if (!m->backend_valid || !ACT_ConfigValid(c))
    {
        s->status = ACT_STATUS_BAD_INPUT;
        s->enabled = 0; s->braking = 1; s->pfm_hz = 0.0f;
        return s->status;
    }
    if (!(dt_s > 0.0f) || dt_s > 0.5f) dt_s = 0.0f; /* first sample or scheduler gap: hold */

    if (m->hardware && !m->gpio_ready)
    {
        if (ACT_GpioOpen(m) != 0)
        {
            s->status = ACT_STATUS_IO_ERROR;
            return s->status;
        }
        s->status = ACT_STATUS_NOT_READY;
        return s->status; /* first real output on the next wakeup, like the sensor apps */
    }

    /* --- direction handling: never flip DIR at speed; go through zero first --- */
    desired_dir = (s->cmd_omega > c->stop_threshold) ? 1 : (s->cmd_omega < -c->stop_threshold) ? -1 : 0;
    goal = s->cmd_omega;
    if (s->direction != 0 && desired_dir != 0 && desired_dir != s->direction)
    {
        goal = 0.0f; /* decelerate; flip below once the wheel is slow */
    }

    /* --- slew limiter --- */
    max_step        = c->slew_rad_s2 * dt_s;
    s->target_omega = ACT_SlewToward(s->target_omega, goal, max_step);

    if (fabsf(s->target_omega) < c->stop_threshold && fabsf(s->meas_omega) < c->stop_threshold)
    {
        /* stopped (or passing through zero): safe moment to set direction */
        s->direction = desired_dir;
    }
    else if (s->direction == 0)
    {
        s->direction = (s->target_omega >= 0.0f) ? 1 : -1;
    }

    /* --- output: frequency from |target|, brake + disable when idle --- */
    drive_omega = fabsf(s->target_omega);
    if (drive_omega < c->stop_threshold)
    {
        s->pfm_hz  = 0.0f;
        s->enabled = 0;
        s->braking = 1;
    }
    else
    {
        s->pfm_hz = drive_omega / c->rad_s_per_hz;
        if (s->pfm_hz < c->pfm_min_hz) s->pfm_hz = c->pfm_min_hz;
        if (s->pfm_hz > c->pfm_max_hz) s->pfm_hz = c->pfm_max_hz;
        s->enabled = 1;
        s->braking = 0;
    }

    /* --- plant: measured if FG exists (TODO), otherwise first-order model --- */
    {
        float model_goal = s->enabled ? (float)s->direction * s->pfm_hz * c->rad_s_per_hz : 0.0f;
        float tau        = s->braking ? c->sim_tau_s * 0.25f : c->sim_tau_s;
        float a          = (dt_s > 0.0f) ? dt_s / (tau + dt_s) : 0.0f;
        m->sim_omega += a * (model_goal - m->sim_omega);
        s->meas_omega = m->sim_omega;
        s->measured   = 0;
    }

    if (m->hardware && ACT_GpioApply(m) != 0)
    {
        s->status = ACT_STATUS_IO_ERROR;
        m->gpio_ready = 0; /* re-init on next wakeup */
        return s->status;
    }

    s->status = s->saturated ? ACT_STATUS_SATURATED : ACT_STATUS_OK;
    return s->status;
}
