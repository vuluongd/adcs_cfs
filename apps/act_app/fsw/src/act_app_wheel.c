/*
 * act_app_wheel.c - cFS glue for the reaction wheel.
 *
 *   SCH_LAB --WAKEUP_ACT (0x18E3, 10 Hz)--> ACT_APP --WHEEL_DATA (0x08E4)--> ADCS_APP / TO_LAB
 *   ADCS_APP / ground --ACT_CMD (0x18E0, CC 4 SET_WHEEL_SPEED / CC 5 STOP_WHEEL)--> ACT_APP
 *
 * Every wakeup: real dt from CLOCK_MONOTONIC -> ACT_MotorStep -> fill payload -> publish.
 */
#define _POSIX_C_SOURCE 200809L
#include "act_app.h"
#include "act_app_wheel.h"
#include "act_data.h"
#include "act_motor.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define ACT_APP_WHEEL_START_EID   50
#define ACT_APP_WHEEL_TX_ERR_EID  51
#define ACT_APP_WHEEL_STATUS_EID  52
#define ACT_APP_WHEEL_PRINT_EVERY 10u /* console line every N wakeups (1 Hz at 10 Hz) */

static ACT_Motor_t        Motor;
static ACT_WheelDataTlm_t Tlm;
static uint32             Counter;
static uint64_t           LastNs;
static bool               HaveLast;
static uint8              LastStatus = 0xFF;

static uint64_t ACT_MonotonicNs(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

CFE_Status_t ACT_APP_WheelInit(void)
{
    CFE_Status_t      status;
    ACT_MotorConfig_t cfg;

    ACT_MotorDefaultConfig(&cfg);
    ACT_MotorInit(&Motor, &cfg);
    Counter    = 0;
    HaveLast   = false;
    LastStatus = 0xFF;

    memset(&Tlm, 0, sizeof(Tlm));
    status = CFE_MSG_Init(CFE_MSG_PTR(Tlm.TelemetryHeader), CFE_SB_ValueToMsgId(ACT_APP_DATA_MID), sizeof(Tlm));
    if (status != CFE_SUCCESS) return status;

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ACT_APP_WAKEUP_MID), ACT_APP_Data.CommandPipe);
    if (status == CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(ACT_APP_WHEEL_START_EID, CFE_EVS_EventType_INFORMATION,
                          "ACT backend=%s Nidec 24H PFM; omega_max=%.0f rad/s slew=%.0f rad/s2 pfm=%.0f..%.0f Hz",
                          Motor.hardware ? "GPIO" : "SIMULATION", (double)cfg.omega_max_rad_s,
                          (double)cfg.slew_rad_s2, (double)cfg.pfm_min_hz, (double)cfg.pfm_max_hz);
    }
    return status;
}

uint8 ACT_APP_WheelSetSpeed(float omega)
{
    return ACT_MotorSetTarget(&Motor, omega);
}

void ACT_APP_WheelStop(void)
{
    ACT_MotorStop(&Motor);
}

void ACT_APP_OnWakeup(void)
{
    ACT_WheelDataPayload_t *p = &Tlm.Payload;
    const ACT_MotorState_t *s = &Motor.st;
    uint64_t                now = ACT_MonotonicNs();
    float                   dt  = HaveLast ? (float)((double)(now - LastNs) * 1e-9) : 0.0f;
    CFE_Status_t            tx;

    LastNs   = now;
    HaveLast = true;

    (void)ACT_MotorStep(&Motor, dt);

    p->cmd_omega_rad_s    = s->cmd_omega;
    p->target_omega_rad_s = s->target_omega;
    p->meas_omega_rad_s   = s->meas_omega;
    p->pfm_hz             = s->pfm_hz;
    p->sample_counter     = ++Counter;
    p->status             = s->status;
    p->valid              = (s->status == ACT_STATUS_OK || s->status == ACT_STATUS_SATURATED) ? 1 : 0;
    p->saturated          = s->saturated;
    p->simulated          = s->simulated;
    p->enabled            = s->enabled;
    p->braking            = s->braking;
    p->direction          = s->direction;
    p->measured           = s->measured;

    if (s->status != LastStatus &&
        (s->status == ACT_STATUS_IO_ERROR || s->status == ACT_STATUS_BAD_INPUT || s->status == ACT_STATUS_FAULT))
    {
        CFE_EVS_SendEvent(ACT_APP_WHEEL_STATUS_EID, CFE_EVS_EventType_ERROR,
                          "ACT wheel status=%u (2=IO error 4=motor fault 5=bad backend/config)",
                          (unsigned int)s->status);
    }
    LastStatus = s->status;

    if (Counter == 1 || Counter % ACT_APP_WHEEL_PRINT_EVERY == 0)
    {
        printf(">>> ACT [%u] cmd=%.1f tgt=%.1f meas=%.1f rad/s pfm=%.0f Hz dir=%d en=%d brk=%d sat=%d valid=%d status=%d sim=%d\n",
               (unsigned int)p->sample_counter, (double)p->cmd_omega_rad_s, (double)p->target_omega_rad_s,
               (double)p->meas_omega_rad_s, (double)p->pfm_hz, (int)p->direction, (int)p->enabled,
               (int)p->braking, (int)p->saturated, (int)p->valid, (int)p->status, (int)p->simulated);
    }

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(Tlm.TelemetryHeader));
    tx = CFE_SB_TransmitMsg(CFE_MSG_PTR(Tlm.TelemetryHeader), true);
    if (tx != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(ACT_APP_WHEEL_TX_ERR_EID, CFE_EVS_EventType_ERROR, "WHEEL_DATA transmit failed: %ld", (long)tx);
    }
}

void ACT_APP_WheelClose(void)
{
    ACT_MotorClose(&Motor);
}
