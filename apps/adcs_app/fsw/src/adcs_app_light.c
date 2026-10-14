#include "adcs_app.h"
#include "adcs_app_light.h"
#include <math.h>
#include <string.h>

void ADCS_APP_ReceiveLight(const CFE_SB_Buffer_t *buffer)
{
    CFE_MSG_Size_t size = 0;
    CFE_TIME_SysTime_t timestamp;
    const LIGHT_DataTlm_t *packet;
    if (CFE_MSG_GetSize(&buffer->Msg, &size) != CFE_SUCCESS || size != sizeof(LIGHT_DataTlm_t) ||
        CFE_MSG_GetMsgTime(&buffer->Msg, &timestamp) != CFE_SUCCESS)
    {
        ADCS_APP_Data.LightReceived = false;
        ADCS_APP_Data.CommandErrorCounter++;
        CFE_EVS_SendEvent(50, CFE_EVS_EventType_ERROR, "ADCS rejected malformed LIGHT_DATA");
        return;
    }
    packet = (const LIGHT_DataTlm_t *)buffer;
    memcpy(&ADCS_APP_Data.Light, &packet->Payload, sizeof(packet->Payload));
    ADCS_APP_Data.LightTimestamp = timestamp;
    ADCS_APP_Data.LightReceived = true;
    if (packet->Payload.sample_counter == 1 || packet->Payload.sample_counter % 10 == 0)
        CFE_EVS_SendEvent(51, CFE_EVS_EventType_INFORMATION,
                          "LIGHT_DATA n=%lu error=%.3f error_valid=%u angle=%.2fdeg angle_valid=%u status=%u sim=%u",
                          (unsigned long)packet->Payload.sample_counter,
                          (double)packet->Payload.normalized_diff,
                          (unsigned int)packet->Payload.error_valid,
                          (double)packet->Payload.bearing_rad * 57.2957795131,
                          (unsigned int)packet->Payload.valid, (unsigned int)packet->Payload.status,
                          (unsigned int)packet->Payload.simulated);
}

bool ADCS_APP_LightErrorUsable(bool allow_simulated)
{
    CFE_TIME_SysTime_t age;
    const LIGHT_DataPayload_t *p = &ADCS_APP_Data.Light;
    if (!ADCS_APP_Data.LightReceived || p->error_valid != 1 ||
        (p->status != LIGHT_STATUS_OK && p->status != LIGHT_STATUS_NO_CALIBRATION &&
         p->status != LIGHT_STATUS_OUT_OF_VIEW) ||
        p->simulated > 1 || (p->simulated && !allow_simulated) ||
        !isfinite(p->normalized_diff) || fabsf(p->normalized_diff) > 1.0f ||
        !isfinite(p->left_lux) || p->left_lux < 0.0f ||
        !isfinite(p->right_lux) || p->right_lux < 0.0f ||
        !isfinite(p->left_lux + p->right_lux) || p->left_lux + p->right_lux <= 0.0f)
        return false;
    /* Header time is publication time. Limit includes SB queue delay. Future time
       underflows the unsigned subtraction and is also rejected. Same CPU clock. */
    age = CFE_TIME_Subtract(CFE_TIME_GetTime(), ADCS_APP_Data.LightTimestamp);
    return age.Seconds == 0 && age.Subseconds <= CFE_TIME_Micro2SubSecs(600000);
}

bool ADCS_APP_LightUsable(bool allow_simulated)
{
    const LIGHT_DataPayload_t *p = &ADCS_APP_Data.Light;
    return ADCS_APP_LightErrorUsable(allow_simulated) && p->valid == 1 &&
           p->status == LIGHT_STATUS_OK && isfinite(p->bearing_rad) &&
           fabsf(p->bearing_rad) < 1.570796327f;
}
