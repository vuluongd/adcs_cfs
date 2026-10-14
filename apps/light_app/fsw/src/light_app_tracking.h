#ifndef LIGHT_APP_TRACKING_H
#define LIGHT_APP_TRACKING_H
#include "cfe.h"
CFE_Status_t LIGHT_APP_TrackingInit(void);
void LIGHT_APP_OnWakeup(void);
void LIGHT_APP_TrackingClose(void);
#endif
