/*
 * Stubs for fsw/src/act_app_wheel.c so the coverage tests of act_app.c /
 * act_app_dispatch.c / act_app_cmds.c link without the motor layer.
 */
#include "cfe.h"
#include "act_app_wheel.h"

CFE_Status_t ACT_APP_WheelInit(void) { return CFE_SUCCESS; }
void         ACT_APP_OnWakeup(void) {}
void         ACT_APP_WheelClose(void) {}
uint8        ACT_APP_WheelSetSpeed(float omega) { (void)omega; return 0; }
void         ACT_APP_WheelStop(void) {}
