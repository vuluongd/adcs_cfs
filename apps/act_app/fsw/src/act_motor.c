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
/* Defaults.  rad_s_per_hz and the PFM limits are PLACEHOLDERS until the bench  */
/* test (docs/act_app) gives real numbers for this motor + flywheel.           */
/* ------------------------------------------------------------------------- */
void ACT_MotorDefaultConfig(ACT_MotorConfig_t *cfg)
{
    *cfg = (ACT_MotorConfig_t){
        .omega_max_rad_s = 300.0f,  /* ~2865 rpm, conservative for a printed flywheel */
        .slew_rad_s2     = 60.0f,   /* 0 -> 300 rad/s in 5 s                           */
        .pfm_min_hz      = 300.0f,  /* forum: motor does not start below ~150-250 Hz    */
        .pfm_max_hz      = 10000.0f,/* forum: self-shutdown above ~15 kHz, keep margin  */
        .rad_s_per_hz    = 0.03f,   /* TODO bench: 10 kHz -> 300 rad/s is a GUESS       */
        .sim_tau_s       = 0.8f,
        .stop_threshold  = 2.0f     /* rad/s                                            */
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
/* gpio backend: Raspberry Pi sysfs.  UNTESTED - written from the forum pinout. */
/* PFM = hardware PWM with variable period and 50 % duty.                      */
/* Requires dtoverlay=pwm (GPIO18 -> pwmchip0/pwm0) and exported GPIOs.        */
/* Env overrides: ACT_APP_PWM_DIR, ACT_APP_GPIO_ENABLE/DIR/BRAKE (BCM numbers).*/
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

static int ACT_GpioWrite(const char *bcm, int level)
{
    char path[96];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%s/value", bcm);
    return ACT_WriteStr(path, level ? "1" : "0");
}

static int ACT_GpioExportOut(const char *bcm)
{
    char path[96];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%s/direction", bcm);
    if (access(path, W_OK) != 0)
    {
        (void)ACT_WriteStr("/sys/class/gpio/export", bcm); /* may already exist */
    }
    return ACT_WriteStr(path, "out");
}

static int ACT_GpioOpen(ACT_Motor_t *m)
{
    const char *pwmdir = ACT_Env("ACT_APP_PWM_DIR", "/sys/class/pwm/pwmchip0/pwm0");
    char path[160];

    if (ACT_GpioExportOut(ACT_Env("ACT_APP_GPIO_ENABLE", "23")) != 0 ||
        ACT_GpioExportOut(ACT_Env("ACT_APP_GPIO_DIR", "24")) != 0 ||
        ACT_GpioExportOut(ACT_Env("ACT_APP_GPIO_BRAKE", "25")) != 0)
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

    if (ACT_GpioWrite(ACT_Env("ACT_APP_GPIO_BRAKE", "25"), !s->braking) != 0) return -1; /* BRAKE low = brake */
    if (ACT_GpioWrite(ACT_Env("ACT_APP_GPIO_DIR", "24"), s->direction > 0) != 0) return -1; /* high = CCW */
    if (ACT_GpioWrite(ACT_Env("ACT_APP_GPIO_ENABLE", "23"), s->enabled) != 0) return -1;

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
