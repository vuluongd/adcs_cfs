/*
 * ADCS_APP outer loop: LIGHT_DATA (góc tới ánh sáng) + IMU_DATA (góc/ tốc độ) --P--> ACT_CMD (tốc độ bánh đà)
 * Vòng trong PI tốc độ bánh đà nằm ở ACT_APP.
 */
#include "adcs_app.h"
#include "adcs_app_loop.h"
#include "adcs_app_imu.h"
#include "adcs_app_light.h"
#include "adcs_app_control.h"
#include "adcs_app_eventids.h"

#include "act_app_msgids.h"
#include "act_app_msg.h"
#include "act_app_fcncodes.h"
#include "act_data.h"

#include <math.h>
#include <string.h>

/* Chỉ gửi lại lệnh khi tốc độ thay đổi quá ngưỡng này: tránh làm ngập EVS (ACT_APP in 1 event cho mỗi lệnh). */
#define ADCS_APP_CMD_MIN_DELTA_RAD_S 1.0f
/* Gửi lại lệnh định kỳ dù không đổi (an toàn nếu ACT_APP khởi động lại), tính theo số chu kỳ 10 Hz. */
#define ADCS_APP_CMD_REFRESH_TICKS   50u

static void SendWheelSpeed(float omega)
{
    ACT_APP_SetWheelSpeedCmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));
    CFE_MSG_Init(CFE_MSG_PTR(cmd.CommandHeader), CFE_SB_ValueToMsgId(ACT_APP_CMD_MID), sizeof(cmd));
    CFE_MSG_SetFcnCode(CFE_MSG_PTR(cmd.CommandHeader), ACT_APP_SET_WHEEL_SPEED_CC);
    cmd.Payload.omega_rad_s = omega;
    CFE_MSG_GenerateChecksum(CFE_MSG_PTR(cmd.CommandHeader));

    if (CFE_SB_TransmitMsg(CFE_MSG_PTR(cmd.CommandHeader), true) != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(ADCS_APP_WHEEL_TX_ERR_EID, CFE_EVS_EventType_ERROR, "ADCS: cannot send SET_WHEEL_SPEED");
        return;
    }
    ADCS_APP_Data.LastOmegaSent = omega;
    ADCS_APP_Data.TicksSinceSend = 0;
}

void ADCS_APP_StopWheel(void)
{
    ACT_APP_StopWheelCmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));
    CFE_MSG_Init(CFE_MSG_PTR(cmd.CommandHeader), CFE_SB_ValueToMsgId(ACT_APP_CMD_MID), sizeof(cmd));
    CFE_MSG_SetFcnCode(CFE_MSG_PTR(cmd.CommandHeader), ACT_APP_STOP_WHEEL_CC);
    CFE_MSG_GenerateChecksum(CFE_MSG_PTR(cmd.CommandHeader));
    (void)CFE_SB_TransmitMsg(CFE_MSG_PTR(cmd.CommandHeader), true);
    ADCS_APP_Data.LastOmegaSent  = 0.0f;
    ADCS_APP_Data.TicksSinceSend = 0;
}

void ADCS_APP_ReceiveWheel(const CFE_SB_Buffer_t *buffer)
{
    CFE_MSG_Size_t size = 0;

    if (CFE_MSG_GetSize(&buffer->Msg, &size) != CFE_SUCCESS || size != sizeof(ACT_WheelDataTlm_t))
    {
        ADCS_APP_Data.WheelReceived = false;
        ADCS_APP_Data.CommandErrorCounter++;
        CFE_EVS_SendEvent(ADCS_APP_WHEEL_MALFORMED_EID, CFE_EVS_EventType_ERROR, "ADCS rejected malformed WHEEL_DATA");
        return;
    }
    memcpy(&ADCS_APP_Data.Wheel, &((const ACT_WheelDataTlm_t *)buffer)->Payload, sizeof(ADCS_APP_Data.Wheel));
    ADCS_APP_Data.WheelReceived = true;
}

bool ADCS_APP_SetMode(uint32 mode)
{
    if (mode > 1u)
    {
        return false;
    }
    if (mode == 0u && ADCS_APP_Data.Mode != 0u)
    {
        ADCS_APP_StopWheel();
    }
    ADCS_Ctrl_Reset(&ADCS_APP_Data.CtrlMem);
    ADCS_APP_Data.CtrlOut.omega_cmd_rad_s = 0.0f;
    ADCS_APP_Data.CtrlOut.error_rad       = 0.0f;
    ADCS_APP_Data.CtrlOut.state           = ADCS_CTRL_IDLE;
    ADCS_APP_Data.Mode                    = (uint8)mode;
    return true;
}

void ADCS_APP_OnWakeup(void)
{
    ADCS_Ctrl_In_t     in;
    ADCS_Ctrl_Out_t    out;
    CFE_TIME_SysTime_t now = CFE_TIME_GetTime();
    float              dt  = 0.1f;
    bool               allow_sim = (ADCS_APP_Data.AllowSimulated != 0u);

    /* dt thật giữa 2 wakeup, kẹp để một lần trễ lớn không làm hỏng bộ đếm hold. */
    if (ADCS_APP_Data.HaveLastTick)
    {
        CFE_TIME_SysTime_t d = CFE_TIME_Subtract(now, ADCS_APP_Data.LastTick);
        dt = (float)d.Seconds + (float)CFE_TIME_Sub2MicroSecs(d.Subseconds) * 1.0e-6f;
        if (!(dt >= 0.01f))  dt = 0.01f;
        if (dt > 0.5f)       dt = 0.5f;
    }
    ADCS_APP_Data.LastTick     = now;
    ADCS_APP_Data.HaveLastTick = true;

    /* Gọi mỗi chu kỳ (đúng yêu cầu trong adcs_app_imu.h / adcs_app_light.h). */
    ADCS_APP_Data.ImuOk   = ADCS_APP_ImuUsable(allow_sim);
    ADCS_APP_Data.LightOk = ADCS_APP_LightUsable(allow_sim);

    if (ADCS_APP_Data.Mode == 0u)
    {
        return; /* DISABLED: không phát lệnh nào */
    }

    memset(&in, 0, sizeof(in));
    in.light_usable  = ADCS_APP_Data.LightOk;
    in.light_counter = ADCS_APP_Data.Light.sample_counter;
    in.bearing_rad   = ADCS_APP_Data.Light.bearing_rad;
    in.imu_usable    = ADCS_APP_Data.ImuOk;
    in.imu_angle_rad = ADCS_APP_Data.Imu.angle_rad;

    ADCS_Ctrl_Step(&ADCS_APP_Data.Params, &ADCS_APP_Data.CtrlMem, &in, dt, &out);

    if (out.state != ADCS_APP_Data.CtrlOut.state)
    {
        CFE_EVS_SendEvent(ADCS_APP_CTRL_STATE_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "ADCS control state %u -> %u (0=IDLE 1=LIGHT 2=IMU_HOLD 3=LOST) err=%.2fdeg",
                          (unsigned int)ADCS_APP_Data.CtrlOut.state, (unsigned int)out.state,
                          (double)out.error_rad * 57.2957795131);
    }
    ADCS_APP_Data.CtrlOut = out;

    if (ADCS_APP_Data.TicksSinceSend < 0xFFFFu)
    {
        ADCS_APP_Data.TicksSinceSend++;
    }
    if (fabsf(out.omega_cmd_rad_s - ADCS_APP_Data.LastOmegaSent) >= ADCS_APP_CMD_MIN_DELTA_RAD_S ||
        (out.omega_cmd_rad_s == 0.0f && ADCS_APP_Data.LastOmegaSent != 0.0f) ||
        ADCS_APP_Data.TicksSinceSend >= ADCS_APP_CMD_REFRESH_TICKS)
    {
        SendWheelSpeed(out.omega_cmd_rad_s);
    }
}

void ADCS_APP_FillHk(void)
{
    ADCS_APP_HkTlm_Payload_t *h = &ADCS_APP_Data.HkTlm.Payload;

    h->Mode           = ADCS_APP_Data.Mode;
    h->CtrlState      = (uint8)ADCS_APP_Data.CtrlOut.state;
    h->ErrorRad       = ADCS_APP_Data.CtrlOut.error_rad;
    h->OmegaCmdRadS   = ADCS_APP_Data.LastOmegaSent;
    h->WheelMeasRadS  = ADCS_APP_Data.WheelReceived ? ADCS_APP_Data.Wheel.meas_omega_rad_s : 0.0f;
    h->Kp             = ADCS_APP_Data.Params.kp;
    h->OmegaLimit     = ADCS_APP_Data.Params.omega_limit_rad_s;
    h->DeadbandRad    = ADCS_APP_Data.Params.deadband_rad;
    h->ImuUsable      = ADCS_APP_Data.ImuOk ? 1 : 0;
    h->LightUsable    = ADCS_APP_Data.LightOk ? 1 : 0;
    h->WheelValid     = (ADCS_APP_Data.WheelReceived && ADCS_APP_Data.Wheel.valid) ? 1 : 0;
    h->AllowSimulated = ADCS_APP_Data.AllowSimulated;
}
