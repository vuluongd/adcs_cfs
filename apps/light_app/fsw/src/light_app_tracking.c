#include "light_app.h"
#include "light_app_tracking.h"
#include "light_sensor.h"
#include "light_tracking.h"
#include <string.h>

static LIGHT_Sensor_t Sensor = {.fd_left = -1, .fd_right = -1};
static LIGHT_Config_t Config;
static LIGHT_Filter_t Filter;
static LIGHT_DataTlm_t Tlm;
static uint32 Counter;

CFE_Status_t LIGHT_APP_TrackingInit(void)
{
    CFE_Status_t status;
    LIGHT_DefaultConfig(&Config);
    LIGHT_ResetFilter(&Filter);
    LIGHT_SensorInit(&Sensor);
    if (!Sensor.hardware) LIGHT_UseSimulationCalibration(&Config);
    Counter = 0;
    memset(&Tlm, 0, sizeof(Tlm));
    status = CFE_MSG_Init(CFE_MSG_PTR(Tlm.TelemetryHeader),
                         CFE_SB_ValueToMsgId(LIGHT_APP_DATA_MID), sizeof(Tlm));
    if (status != CFE_SUCCESS) return status;
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LIGHT_APP_WAKEUP_MID), LIGHT_APP_Data.CommandPipe);
    if (status == CFE_SUCCESS)
        CFE_EVS_SendEvent(50, CFE_EVS_EventType_INFORMATION,
                          "LIGHT backend=%s, parallel sensors+baffle; 5Hz; sim table only in SIMULATION",
                          Sensor.hardware ? "I2C" : "SIMULATION");
    return status;
}

void LIGHT_APP_OnWakeup(void)
{
    float left, right;
    uint8_t status = LIGHT_SensorRead(&Sensor, &left, &right);
    CFE_Status_t tx_status;
    LIGHT_Estimate(&Config, &Filter, left, right, status, &Tlm.Payload);
    Tlm.Payload.sample_counter = ++Counter;
    Tlm.Payload.simulated = Sensor.hardware ? 0 : 1;
    CFE_SB_TimeStampMsg(CFE_MSG_PTR(Tlm.TelemetryHeader));
    tx_status = CFE_SB_TransmitMsg(CFE_MSG_PTR(Tlm.TelemetryHeader), true);
    if (tx_status != CFE_SUCCESS)
        CFE_EVS_SendEvent(51, CFE_EVS_EventType_ERROR, "LIGHT_DATA transmit failed: %ld", (long)tx_status);
}

void LIGHT_APP_TrackingClose(void)
{
    LIGHT_SensorClose(&Sensor);
}
