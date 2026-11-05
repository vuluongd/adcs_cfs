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

/* Calibration table: normalized_diff -> bearing_rad */
/* Negative angles -10 to -30 are extrapolated values. */

#define LIGHT_DEG_TO_RAD 0.01745329252f
#define LIGHT_RAD(deg) ((deg) * LIGHT_DEG_TO_RAD)

static const LIGHT_CalPoint_t RealCalibration[] = {
    {-0.940f, LIGHT_RAD(-30.0f)},
    {-0.774f, LIGHT_RAD(-25.0f)},
    {-0.608f, LIGHT_RAD(-20.0f)},
    {-0.442f, LIGHT_RAD(-15.0f)},
    {-0.276f, LIGHT_RAD(-10.0f)},
    {-0.110f, LIGHT_RAD( -5.0f)},
    { 0.056f, LIGHT_RAD(  0.0f)},
    { 0.131f, LIGHT_RAD(  5.0f)},
    { 0.170f, LIGHT_RAD( 10.0f)},
    { 0.258f, LIGHT_RAD( 15.0f)},
    { 0.370f, LIGHT_RAD( 20.0f)},
    { 0.580f, LIGHT_RAD( 25.0f)},
    { 0.960f, LIGHT_RAD( 30.0f)}
};

CFE_Status_t LIGHT_APP_TrackingInit(void)
{
    CFE_Status_t status;
    LIGHT_DefaultConfig(&Config);
    LIGHT_ResetFilter(&Filter);
 
LIGHT_SensorInit(&Sensor);

if (Sensor.hardware)
{
    /* Use measured calibration for real sensors */
    Config.calibration = RealCalibration;
    Config.calibration_count =
        sizeof(RealCalibration) / sizeof(RealCalibration[0]);
}
else
{
    /* Keep the original simulation calibration */
    LIGHT_UseSimulationCalibration(&Config);
}

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

    /* ===== IN RA CONSOLE ===== */
    printf(">>> LIGHT [%u]  L=%.1f  R=%.1f  diff=%.3f  angle=%.1f deg  valid=%d  status=%d  sim=%d\n",
           Tlm.Payload.sample_counter,
           Tlm.Payload.left_lux,
           Tlm.Payload.right_lux,
           Tlm.Payload.normalized_diff,
           Tlm.Payload.bearing_rad * 57.2957795f,
           Tlm.Payload.valid,
           Tlm.Payload.status,
           Tlm.Payload.simulated);
    /* ======================== */

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(Tlm.TelemetryHeader));
    tx_status = CFE_SB_TransmitMsg(CFE_MSG_PTR(Tlm.TelemetryHeader), true);
    if (tx_status != CFE_SUCCESS)
        CFE_EVS_SendEvent(51, CFE_EVS_EventType_ERROR, "LIGHT_DATA transmit failed: %ld", (long)tx_status);
}

void LIGHT_APP_TrackingClose(void)
{
    LIGHT_SensorClose(&Sensor);
}
