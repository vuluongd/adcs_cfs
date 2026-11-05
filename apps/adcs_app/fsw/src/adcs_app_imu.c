#include "adcs_app.h"
#include "adcs_app_imu.h"
#include "adcs_app_eventids.h"
#include "imu_status.h"
#include <math.h>
#include <string.h>

/* MPU6050 is configured for +-250 deg/s: anything beyond that is a corrupted sample. */
#define ADCS_APP_IMU_MAX_RATE_RAD_S 4.3633231f

void ADCS_APP_ReceiveImu(const CFE_SB_Buffer_t *buffer)
{
    CFE_MSG_Size_t      size = 0;
    CFE_TIME_SysTime_t  timestamp;
    const IMU_DataTlm_t *packet;

    if (CFE_MSG_GetSize(&buffer->Msg, &size) != CFE_SUCCESS || size != sizeof(IMU_DataTlm_t) ||
        CFE_MSG_GetMsgTime(&buffer->Msg, &timestamp) != CFE_SUCCESS)
    {
        ADCS_APP_Data.ImuReceived = false;
        ADCS_APP_Data.CommandErrorCounter++;
        CFE_EVS_SendEvent(ADCS_APP_IMU_MALFORMED_EID, CFE_EVS_EventType_ERROR, "ADCS rejected malformed IMU_DATA");
        return;
    }

    packet = (const IMU_DataTlm_t *)buffer;
    memcpy(&ADCS_APP_Data.Imu, &packet->Payload, sizeof(packet->Payload));
    ADCS_APP_Data.ImuTimestamp = timestamp;
    ADCS_APP_Data.ImuReceived  = true;

    if (packet->Payload.sample_counter == 1 || packet->Payload.sample_counter % 10 == 0)
    {
        CFE_EVS_SendEvent(ADCS_APP_IMU_DATA_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "IMU_DATA n=%lu rate=%.3fdeg/s angle=%.2fdeg bias=%.4fdeg/s valid=%u status=%u still=%u sim=%u",
                          (unsigned long)packet->Payload.sample_counter,
                          (double)packet->Payload.rate_rad_s * 57.2957795131,
                          (double)packet->Payload.angle_rad * 57.2957795131,
                          (double)packet->Payload.bias_rad_s * 57.2957795131,
                          (unsigned int)packet->Payload.valid, (unsigned int)packet->Payload.status,
                          (unsigned int)packet->Payload.stationary, (unsigned int)packet->Payload.simulated);
    }
}

bool ADCS_APP_ImuUsable(bool allow_simulated)
{
    CFE_TIME_SysTime_t       age;
    const IMU_DataPayload_t *p = &ADCS_APP_Data.Imu;

    if (!ADCS_APP_Data.ImuReceived || p->valid != 1 || p->status != IMU_STATUS_OK || p->stationary > 1 ||
        p->simulated > 1 || (p->simulated && !allow_simulated) || !isfinite(p->angle_rad) ||
        !isfinite(p->rate_rad_s) || !isfinite(p->bias_rad_s) || !isfinite(p->raw_rate_rad_s) ||
        fabsf(p->rate_rad_s) > ADCS_APP_IMU_MAX_RATE_RAD_S)
    {
        return false;
    }

    /* Header time is publication time. Same CPU clock as ADCS. A time in the future
       underflows the unsigned subtraction and is rejected as well. */
    age = CFE_TIME_Subtract(CFE_TIME_GetTime(), ADCS_APP_Data.ImuTimestamp);
    return age.Seconds == 0 && age.Subseconds <= CFE_TIME_Micro2SubSecs(ADCS_APP_IMU_MAX_AGE_US);
}
