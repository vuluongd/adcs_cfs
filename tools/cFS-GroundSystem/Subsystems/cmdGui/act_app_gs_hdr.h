#define ACT_APP_NOOP_CC              0
#define ACT_APP_RESET_COUNTERS_CC    1
#define ACT_APP_SET_WHEEL_SPEED_CC   4
#define ACT_APP_STOP_WHEEL_CC        5
#define ACT_APP_SET_CONTROL_MODE_CC  6

typedef struct
{
    uint8   CmdHeader[CFE_SB_CMD_HDR_SIZE];
    uint32  Mode;
} ACT_APP_SetControlMode_t;

typedef struct
{
    uint8   CmdHeader[CFE_SB_CMD_HDR_SIZE];
    float   omega_rad_s;
} ACT_APP_SetWheelSpeed_t;
