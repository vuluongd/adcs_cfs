/* Unit test for the receiver with a small cFE boundary double.
 * Real cFE headers/ABI are checked separately by the full mission build. */
#include "light_tracking.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef int CFE_Status_t;
typedef size_t CFE_MSG_Size_t;
typedef struct { uint32_t Seconds, Subseconds; } CFE_TIME_SysTime_t;
typedef struct { size_t size; CFE_TIME_SysTime_t timestamp; } CFE_MSG_Message_t;
typedef struct { CFE_MSG_Message_t Msg; } CFE_SB_Buffer_t;
typedef struct
{
    CFE_MSG_Message_t TelemetryHeader;
    LIGHT_DataPayload_t Payload;
} LIGHT_DataTlm_t;
static struct
{
    LIGHT_DataPayload_t Light;
    CFE_TIME_SysTime_t LightTimestamp;
    bool LightReceived;
    uint8_t CommandErrorCounter;
} ADCS_APP_Data;

#define CFE_SUCCESS 0
#define CFE_EVS_EventType_ERROR 2
#define CFE_EVS_EventType_INFORMATION 1
static CFE_TIME_SysTime_t Now;
static int time_error;
static CFE_Status_t CFE_MSG_GetSize(const CFE_MSG_Message_t *m, CFE_MSG_Size_t *size)
{ *size = m->size; return 0; }
static CFE_Status_t CFE_MSG_GetMsgTime(const CFE_MSG_Message_t *m, CFE_TIME_SysTime_t *t)
{ *t = m->timestamp; return time_error; }
static CFE_TIME_SysTime_t CFE_TIME_GetTime(void) { return Now; }
static uint32_t CFE_TIME_Micro2SubSecs(uint32_t us)
{ return (uint32_t)(((uint64_t)us << 32) / 1000000); }
static CFE_TIME_SysTime_t CFE_TIME_Subtract(CFE_TIME_SysTime_t a, CFE_TIME_SysTime_t b)
{
    uint64_t x = ((uint64_t)a.Seconds << 32) | a.Subseconds;
    uint64_t y = ((uint64_t)b.Seconds << 32) | b.Subseconds;
    uint64_t d = x - y;
    CFE_TIME_SysTime_t r = {(uint32_t)(d >> 32), (uint32_t)d};
    return r;
}
static void CFE_EVS_SendEvent(int id, int type, const char *format, ...)
{ (void)id; (void)type; (void)format; }

#define ADCS_APP_H
#define ADCS_APP_LIGHT_H
#include "../../adcs_app/fsw/src/adcs_app_light.c"

int main(void)
{
    LIGHT_DataTlm_t p = {0};
    p.TelemetryHeader.size = sizeof(p);
    p.TelemetryHeader.timestamp.Seconds = 10;
    p.Payload = (LIGHT_DataPayload_t){700, 700, 0, 0, 1, 1, LIGHT_STATUS_OK, 0, 1};
    Now.Seconds = 10;
    assert(!ADCS_APP_LightUsable(false));
    ADCS_APP_ReceiveLight((const CFE_SB_Buffer_t *)&p);
    assert(ADCS_APP_LightUsable(false));
    Now.Subseconds = CFE_TIME_Micro2SubSecs(500000);
    assert(ADCS_APP_LightUsable(false));
    Now.Subseconds = CFE_TIME_Micro2SubSecs(700000);
    assert(!ADCS_APP_LightUsable(false));
    Now.Seconds = 11; Now.Subseconds = 0;
    assert(!ADCS_APP_LightUsable(false));
    Now.Seconds = 9; /* Future publication timestamp must not be trusted. */
    assert(!ADCS_APP_LightUsable(false));
    Now.Seconds = 10;
    p.Payload.simulated = 1;
    ADCS_APP_ReceiveLight((const CFE_SB_Buffer_t *)&p);
    assert(!ADCS_APP_LightUsable(false) && ADCS_APP_LightUsable(true));
    p.Payload.simulated = 0; p.Payload.valid = 0;
    ADCS_APP_ReceiveLight((const CFE_SB_Buffer_t *)&p);
    assert(!ADCS_APP_LightUsable(true));
    p.Payload.valid = 1; p.Payload.status = LIGHT_STATUS_IO_ERROR;
    ADCS_APP_ReceiveLight((const CFE_SB_Buffer_t *)&p);
    assert(!ADCS_APP_LightUsable(true));
    p.Payload.status = LIGHT_STATUS_OK; p.Payload.bearing_rad = NAN;
    ADCS_APP_ReceiveLight((const CFE_SB_Buffer_t *)&p);
    assert(!ADCS_APP_LightUsable(true));
    p.Payload.bearing_rad = 0;
    p.TelemetryHeader.size--;
    ADCS_APP_ReceiveLight((const CFE_SB_Buffer_t *)&p);
    assert(!ADCS_APP_Data.LightReceived);
    p.TelemetryHeader.size++;
    time_error = -1;
    ADCS_APP_ReceiveLight((const CFE_SB_Buffer_t *)&p);
    assert(!ADCS_APP_Data.LightReceived);
    time_error = 0;
    p.Payload.valid = 0;
    p.Payload.error_valid = 1;
    p.Payload.status = LIGHT_STATUS_NO_CALIBRATION;
    p.Payload.bearing_rad = 0;
    ADCS_APP_ReceiveLight((const CFE_SB_Buffer_t *)&p);
    assert(ADCS_APP_LightErrorUsable(false) && !ADCS_APP_LightUsable(false));
    p.Payload.status = LIGHT_STATUS_OUT_OF_VIEW;
    ADCS_APP_ReceiveLight((const CFE_SB_Buffer_t *)&p);
    assert(ADCS_APP_LightErrorUsable(false) && !ADCS_APP_LightUsable(false));
    p.Payload.normalized_diff = NAN;
    ADCS_APP_ReceiveLight((const CFE_SB_Buffer_t *)&p);
    assert(!ADCS_APP_LightErrorUsable(false));
    puts("PASS: receive/freshness; separate error and angle validity; no-calibration; malformed packets");
    return 0;
}
