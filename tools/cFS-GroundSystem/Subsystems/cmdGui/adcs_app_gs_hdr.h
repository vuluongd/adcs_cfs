#define ADCS_APP_NOOP_CC            0
#define ADCS_APP_RESET_COUNTERS_CC  1
#define ADCS_APP_SET_MODE_CC        4
#define ADCS_APP_SET_PARAMS_CC      5

typedef struct
{
    uint8   CmdHeader[CFE_SB_CMD_HDR_SIZE];
    uint32  Mode;            /* 0 = DISABLED, 1 = TRACKING */
} ADCS_APP_SetMode_t;

typedef struct
{
    uint8   CmdHeader[CFE_SB_CMD_HDR_SIZE];
    float   Kp;              /* rad/s per rad */
    float   OmegaLimit;      /* rad/s, 0 < x <= 300 */
    float   DeadbandRad;     /* rad */
    uint32  AllowSimulated;  /* 0 / 1 */
} ADCS_APP_SetParams_t;
