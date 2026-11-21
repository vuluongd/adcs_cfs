/*
 * act_motor.c - reaction-wheel motor layer (see act_motor.h).
 *
 * The control logic (clamp, slew, direction reversal, brake) is identical for both
 * backends; only "write the output" and "read the speed" differ.  Nothing in here
 * sleeps or blocks: the cFS wakeup handler calls ACT_MotorStep() at 10 Hz.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <errno.h>
#include "act_motor.h"
#include <termios.h>
#include <stdint.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ------------------------------------------------------------------------- */
/* Defaults.  pwm_freq_hz, the duty limits and rad_s_per_duty are PLACEHOLDERS */
/* until the bench test (docs/act_app) gives real numbers for the new PWM      */
/* driver + flywheel.                                                           */
/* ------------------------------------------------------------------------- */
void ACT_MotorDefaultConfig(ACT_MotorConfig_t *cfg)
{
    *cfg = (ACT_MotorConfig_t){
        .omega_max_rad_s = 300.0f,  /* ~2865 rpm, conservative for a printed flywheel */
        .slew_rad_s2     = 60.0f,   /* 0 -> 300 rad/s in 5 s                           */
        .pwm_freq_hz     = 20000.0f,/* TODO bench: fixed carrier, set to driver spec    */
        .duty_min_pct    = 10.0f,   /* TODO bench: dead zone, motor does not start below */
        .duty_max_pct    = 95.0f,   /* keep margin below 100 %                          */
        .rad_s_per_duty  = 3.0f,    /* TODO bench: 100 % -> 300 rad/s is a GUESS        */
        .sim_tau_s       = 0.8f,
        .stop_threshold  = 2.0f,    /* rad/s                                            */
        /*PI gain (output unit = % duty)*/
        .use_pi = 0, /* 0 = open-loop (mặc định), 1 = PI*/
        .kp              = 0.15f,   /* % per rad/s, ~0.45 x feedforward slope (1/3)     */
        .ki              = 0.08f,
        .integral_max    = 20.0f,   /* % duty                                           */
        .use_encoder        = 0,       /* bật sau khi RL78 chạy ổn */
        .enc_counts_per_rev = 100.0f,
        .enc_lpf_alpha      = 0.3f
    };
}

static int ACT_ConfigValid(const ACT_MotorConfig_t *c)
{
    return isfinite(c->omega_max_rad_s) && c->omega_max_rad_s > 0.0f &&
           isfinite(c->slew_rad_s2) && c->slew_rad_s2 > 0.0f &&
           isfinite(c->pwm_freq_hz) && c->pwm_freq_hz > 0.0f &&
           isfinite(c->duty_min_pct) && c->duty_min_pct > 0.0f &&
           isfinite(c->duty_max_pct) && c->duty_max_pct > c->duty_min_pct && c->duty_max_pct <= 100.0f &&
           isfinite(c->rad_s_per_duty) && c->rad_s_per_duty > 0.0f &&
           isfinite(c->sim_tau_s) && c->sim_tau_s > 0.0f &&
           isfinite(c->stop_threshold) && c->stop_threshold >= 0.0f &&
           isfinite(c->kp) && c->kp >= 0.0f &&
           isfinite(c->ki) && c->ki >= 0.0f &&
           isfinite(c->integral_max) && c->integral_max > 0.0f && 
           isfinite(c->enc_counts_per_rev) && c->enc_counts_per_rev > 0.0f &&
           isfinite(c->enc_lpf_alpha) && c->enc_lpf_alpha > 0.0f && c->enc_lpf_alpha <= 1.0f;
}

/* ------------------------------------------------------------------------- */
/* gpio backend: Raspberry Pi sysfs.  UNTESTED.                                */
/* PWM = hardware PWM with FIXED period (cfg.pwm_freq_hz) and variable duty.   */
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

/* A control line can be left unconnected: set ACT_APP_GPIO_ENABLE/DIR/BRAKE to "none" (or "-1"). */
static int ACT_PinUnused(const char *bcm)
{
    return strcmp(bcm, "none") == 0 || strcmp(bcm, "-1") == 0 || strcmp(bcm, "off") == 0;
}

static int ACT_GpioWrite(const char *bcm, int level)
{
    char path[96];
    if (ACT_PinUnused(bcm)) return 0;
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%s/value", bcm);
    return ACT_WriteStr(path, level ? "1" : "0");
}

static int ACT_GpioExportOut(const char *bcm)
{
    char path[96];
    if (ACT_PinUnused(bcm)) return 0;
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%s/direction", bcm);
    if (access(path, W_OK) != 0)
    {
        (void)ACT_WriteStr("/sys/class/gpio/export", bcm); /* may already exist */
    }
    if (ACT_WriteStr(path, "out") != 0)
    {
        /* On newer Pi OS kernels sysfs GPIO numbers have an offset: see /sys/class/gpio/gpiochip*\/base */
        fprintf(stderr, "ACT: GPIO %s: cannot set direction (%s) - %s missing/unwritable; check gpiochip base offset\n",
                bcm, strerror(errno), path);
        return -1;
    }
    return 0;
}

/* Close any fds left from a previous (failed) open so repeated retries cannot leak. */
static void ACT_PwmCloseFds(ACT_Motor_t *m)
{
    if (m->pwm_fd_period >= 0) close(m->pwm_fd_period);
    if (m->pwm_fd_duty >= 0) close(m->pwm_fd_duty);
    m->pwm_fd_period = m->pwm_fd_duty = -1;
}

/* pwmN only appears after writing N to pwmchipX/export. */
static int ACT_PwmExport(const char *pwmdir)
{
    char path[160];
    int  i;

    snprintf(path, sizeof(path), "%s/period", pwmdir);
    if (access(path, W_OK) == 0) return 0;                      /* already exported */

    snprintf(path, sizeof(path), "%s/export", ACT_Env("ACT_APP_PWM_CHIP", "/sys/class/pwm/pwmchip0"));
    if (ACT_WriteStr(path, ACT_Env("ACT_APP_PWM_CH", "0")) != 0)
    {
        fprintf(stderr, "ACT: PWM export failed (%s): %s - is dtoverlay=pwm enabled in /boot/firmware/config.txt "
                        "(or /boot/config.txt)? chip path wrong? set ACT_APP_PWM_CHIP / ACT_APP_PWM_CH\n",
                path, strerror(errno));
        return -1;
    }
    snprintf(path, sizeof(path), "%s/period", pwmdir);
    for (i = 0; i < 20; i++)                                    /* up to ~1 s for udev permissions */
    {
        if (access(path, W_OK) == 0) return 0;
        usleep(50000);
    }
    fprintf(stderr, "ACT: PWM channel exported but %s not writable (check ACT_APP_PWM_DIR)\n", path);
    return -1;
}

static int ACT_GpioOpen(ACT_Motor_t *m)
{
    const char *pwmdir = ACT_Env("ACT_APP_PWM_DIR", "/sys/class/pwm/pwmchip0/pwm0");
    char path[160];

    ACT_PwmCloseFds(m);

    if (ACT_GpioExportOut(ACT_Env("ACT_APP_GPIO_ENABLE", "23")) != 0 ||
        ACT_GpioExportOut(ACT_Env("ACT_APP_GPIO_DIR", "24")) != 0 ||
        ACT_GpioExportOut(ACT_Env("ACT_APP_GPIO_BRAKE", "25")) != 0)
        return -1;

    if (ACT_PwmExport(pwmdir) != 0) return -1;

    snprintf(path, sizeof(path), "%s/period", pwmdir);
    m->pwm_fd_period = open(path, O_WRONLY | O_CLOEXEC);
    snprintf(path, sizeof(path), "%s/duty_cycle", pwmdir);
    m->pwm_fd_duty = open(path, O_WRONLY | O_CLOEXEC);
    if (m->pwm_fd_period < 0 || m->pwm_fd_duty < 0)
    {
        fprintf(stderr, "ACT: cannot open PWM period/duty_cycle under %s: %s\n", pwmdir, strerror(errno));
        ACT_PwmCloseFds(m);
        return -1;
    }

    snprintf(path, sizeof(path), "%s/enable", pwmdir);
    (void)ACT_WriteStr(path, "0");

    /* Fixed carrier: program the period once, while the channel is disabled. */
    m->pwm_period_ns = (unsigned long)(1.0e9f / m->cfg.pwm_freq_hz);
    if (m->pwm_period_ns == 0ul) { ACT_PwmCloseFds(m); return -1; }
    if (dprintf(m->pwm_fd_duty, "0") < 0 ||                           /* duty <= period always */
        dprintf(m->pwm_fd_period, "%lu", m->pwm_period_ns) < 0)
    {
        fprintf(stderr, "ACT: cannot program PWM period %lu ns: %s\n", m->pwm_period_ns, strerror(errno));
        ACT_PwmCloseFds(m);
        return -1;
    }

    m->gpio_ready = 1;
    return 0;
}

/* Best effort, ignores errors: PWM off, ENABLE low, BRAKE asserted (low). Used on I/O error and on close. */
static void ACT_GpioSafeOff(void)
{
    const char *pwmdir = ACT_Env("ACT_APP_PWM_DIR", "/sys/class/pwm/pwmchip0/pwm0");
    char path[160];

    snprintf(path, sizeof(path), "%s/enable", pwmdir);
    (void)ACT_WriteStr(path, "0");
    (void)ACT_GpioWrite(ACT_Env("ACT_APP_GPIO_ENABLE", "23"), 0);
    (void)ACT_GpioWrite(ACT_Env("ACT_APP_GPIO_BRAKE", "25"), 0);
}

static void ACT_EnvFloat(const char *name, float *dst)
{
    const char *v = getenv(name);
    char *end = NULL;
    float f;
    if (v == NULL || v[0] == '\0') return;
    f = strtof(v, &end);
    if (end != v && *end == '\0') *dst = f;
    else fprintf(stderr, "ACT: ignoring bad value for %s='%s'\n", name, v);
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
    if (s->duty_pct > 0.0f)
    {
        float         d       = (s->duty_pct > 100.0f) ? 100.0f : s->duty_pct;
        unsigned long high_ns = (unsigned long)((double)m->pwm_period_ns * (double)d / 100.0);
        if (high_ns > m->pwm_period_ns) high_ns = m->pwm_period_ns;   /* duty must stay <= period */
        snprintf(buf, sizeof(buf), "%lu", high_ns);
        if (dprintf(m->pwm_fd_duty, "%s", buf) < 0) return -1;
        return ACT_WriteStr(path, "1");
    }
    return ACT_WriteStr(path, "0");
}

/* ------------------------------------------------------------------------- */
static int ACT_EncOpen(ACT_Motor_t *m)
{
    struct termios t;
    m->enc_fd = open(ACT_Env("ACT_APP_ENC_DEV", "/dev/serial0"),
                     O_RDONLY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (m->enc_fd < 0) return -1;
    if (tcgetattr(m->enc_fd, &t) != 0) { close(m->enc_fd); m->enc_fd = -1; return -1; }
    cfmakeraw(&t);
    cfsetispeed(&t, B115200);
    t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0;
    if (tcsetattr(m->enc_fd, TCSANOW, &t) != 0) { close(m->enc_fd); m->enc_fd = -1; return -1; }
    return 0;
}

/* Trả về 1 = có số đếm mới, 0 = chưa có frame mới, -1 = lỗi I/O */
static int ACT_EncRead(ACT_Motor_t *m, uint32_t *count)
{
    uint8_t buf[128];
    int     got = 0;
    ssize_t n;

    while ((n = read(m->enc_fd, buf, sizeof(buf))) > 0)   /* đọc hết hàng đợi, lấy frame mới nhất */
    {
        for (ssize_t i = n - 6; i >= 0; i--)
        {
            if (buf[i] == 0xA5 &&
                (uint8_t)(buf[i] ^ buf[i+1] ^ buf[i+2] ^ buf[i+3] ^ buf[i+4]) == buf[i+5])
            {
                *count = (uint32_t)buf[i+1] | ((uint32_t)buf[i+2] << 8) |
                         ((uint32_t)buf[i+3] << 16) | ((uint32_t)buf[i+4] << 24);
                got = 1;
                break;
            }
        }
    }
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) return -1;
    return got;
}

static int ACT_EncUpdate(ACT_Motor_t *m, float dt_s)
{
    uint32_t cnt = 0;
    int      r   = ACT_EncRead(m, &cnt);

    if (r < 0) return -1;
    m->enc_dt_acc += dt_s;
    if (r == 0) return (m->enc_have_last && ++m->enc_stale <= 5) ? 0 : -1;   /* quá 0.5 s không có frame => coi như mất */
    m->enc_stale = 0;

    if (!m->enc_have_last)
    {
        m->enc_last_count = cnt; m->enc_have_last = 1; m->enc_dt_acc = 0.0f;
        return -1;
    }
    if (m->enc_dt_acc > 0.0f)
    {
        int32_t delta = (int32_t)(cnt - m->enc_last_count);      /* chịu được tràn số */
        float   w     = 6.2831853f * (float)delta / (m->cfg.enc_counts_per_rev * m->enc_dt_acc);
        if (m->st.direction < 0) w = -w;                          /* encoder 1 kênh: lấy dấu từ lệnh */
        m->omega_filt += m->cfg.enc_lpf_alpha * (w - m->omega_filt);
    }
    m->enc_last_count = cnt;
    m->enc_dt_acc     = 0.0f;
    return 0;
}
void ACT_MotorInit(ACT_Motor_t *m, const ACT_MotorConfig_t *cfg)
{
    const char *backend = getenv("ACT_APP_BACKEND");

    memset(m, 0, sizeof(*m));
    m->pwm_fd_period = m->pwm_fd_duty = -1;
    m->enc_fd = -1;
    if (cfg != NULL) m->cfg = *cfg; else ACT_MotorDefaultConfig(&m->cfg);

    /* Bench overrides, so tuning does not need a rebuild (validated by ACT_ConfigValid). */
    ACT_EnvFloat("ACT_APP_PWM_FREQ_HZ",       &m->cfg.pwm_freq_hz);
    ACT_EnvFloat("ACT_APP_DUTY_MIN_PCT",      &m->cfg.duty_min_pct);
    ACT_EnvFloat("ACT_APP_DUTY_MAX_PCT",      &m->cfg.duty_max_pct);
    ACT_EnvFloat("ACT_APP_RAD_S_PER_DUTY",    &m->cfg.rad_s_per_duty);

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
    m->st.duty_pct  = 0.0f;
    m->st.enabled   = 0;
    m->st.braking   = 1;
    if (m->hardware && m->gpio_ready) (void)ACT_GpioApply(m);
    if (m->hardware) ACT_GpioSafeOff();   /* even if the last apply failed: never leave the PWM running */
    ACT_PwmCloseFds(m);
    if (m->enc_fd >= 0) close(m->enc_fd);
    m->enc_fd = -1;
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
        s->enabled = 0; s->braking = 1; s->duty_pct = 0.0f;
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
    /* --- encoder: đo tốc độ thật trước khi điều khiển --- */
    if (c->use_encoder && m->hardware)
    {
        if (m->enc_fd < 0) (void)ACT_EncOpen(m);
        s->measured = (m->enc_fd >= 0 && ACT_EncUpdate(m, dt_s) == 0) ? 1 : 0;
        if (s->measured) s->meas_omega = m->omega_filt;
    }
    else
    {
        s->measured = 0;
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

    /* --- output: PWM duty from |target|, brake + disable when idle --- */
    drive_omega = fabsf(s->target_omega);
    if (drive_omega < c->stop_threshold)
    {
        s->duty_pct = 0.0f;
        s->enabled = 0;
        s->braking = 1;
        m->integral = 0.0f;
    }

    else
    {
        if (c->use_pi && s->measured) {
            float ff = drive_omega / c->rad_s_per_duty;          /* % duty */
            float error = drive_omega - fabsf(s->meas_omega);
            float u     = ff + c->kp * error + m->integral;
            int   sat_hi = (u >= c->duty_max_pct);
            int   sat_lo = (u <= c->duty_min_pct);

            /* Anti-windup đơn giản */
            if (!((sat_hi && error > 0.0f) || (sat_lo && error < 0.0f)))
                m->integral += c->ki * error * dt_s;
            if (m->integral >  c->integral_max) m->integral =  c->integral_max;
            if (m->integral < -c->integral_max) m->integral = -c->integral_max;

            s->duty_pct = ff + c->kp * error + m->integral;
        }
        else
        {
            s->duty_pct = drive_omega / c->rad_s_per_duty;
            m->integral = 0.0f;
        }
        if (s->duty_pct < c->duty_min_pct) s->duty_pct = c->duty_min_pct;
        if (s->duty_pct > c->duty_max_pct) s->duty_pct = c->duty_max_pct;
        s->enabled = 1;
        s->braking = 0;
    }

    /* --- plant: measured if FG exists (TODO), otherwise first-order model --- */
    if (s->measured)
    {
        m->sim_omega = s->meas_omega;
    }
    else
    {
        float model_goal = s->enabled ? (float)s->direction * s->duty_pct * c->rad_s_per_duty : 0.0f;
        float tau        = s->braking ? c->sim_tau_s * 0.25f : c->sim_tau_s;
        float a          = (dt_s > 0.0f) ? dt_s / (tau + dt_s) : 0.0f;
        m->sim_omega += a * (model_goal - m->sim_omega);
        s->meas_omega = m->sim_omega;
    }
    if (m->hardware && ACT_GpioApply(m) != 0)
    {
        ACT_GpioSafeOff();                 /* do not leave the last duty running on an I/O error */
        s->status = ACT_STATUS_IO_ERROR;
        m->gpio_ready = 0; /* re-init on next wakeup */
        return s->status;
    }

    s->status = s->saturated ? ACT_STATUS_SATURATED : ACT_STATUS_OK;
    return s->status;
}
uint8_t ACT_MotorSetMode(ACT_Motor_t *m, uint8_t use_pi)
{
    if (use_pi > 1u) return ACT_STATUS_BAD_INPUT;
    if (use_pi == 1u && !m->st.measured) return ACT_STATUS_BAD_INPUT; /* chưa có encoder thật */
    m->cfg.use_pi = use_pi;
    m->integral   = 0.0f;   /* đổi chế độ thì xóa tích phân */
    return ACT_STATUS_OK;
}