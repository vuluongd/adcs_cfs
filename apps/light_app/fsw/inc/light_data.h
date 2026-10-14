#ifndef LIGHT_DATA_H
#define LIGHT_DATA_H

#include "cfe.h"
#include "light_tracking.h"

typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    LIGHT_DataPayload_t Payload;
} LIGHT_DataTlm_t;

typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
} LIGHT_WakeupCmd_t;

#endif
