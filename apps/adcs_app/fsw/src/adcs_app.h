/************************************************************************
 * NASA Docket No. GSC-19,200-1, and identified as "cFS Draco"
 *
 * Copyright (c) 2023 United States Government as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 * All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ************************************************************************/

/**
 * @file
 *
 * Main header file for the Sample application
 */

#ifndef ADCS_APP_H
#define ADCS_APP_H

/*
** Required header files.
*/
#include "cfe.h"
#include "cfe_config.h"

#include "adcs_app_mission_cfg.h"
#include "adcs_app_platform_cfg.h"

#include "adcs_app_perfids.h"
#include "adcs_app_msgids.h"
#include "adcs_app_msg.h"
#include "light_data.h"

/************************************************************************
** Type Definitions
*************************************************************************/

/*
** Global Data
*/
typedef struct
{
    /*
    ** Command interface counters...
    */
    uint8 CommandCounter;
    uint8 CommandErrorCounter;

    /*
    ** Housekeeping telemetry packet...
    */
    ADCS_APP_HkTlm_t HkTlm;

    LIGHT_DataPayload_t Light;
    CFE_TIME_SysTime_t LightTimestamp;
    bool LightReceived;

    /*
    ** Run Status variable used in the main processing loop
    */
    uint32 RunStatus;

    /*
    ** Operational data (not reported in housekeeping)...
    */
    CFE_SB_PipeId_t CommandPipe;

    CFE_TBL_Handle_t TblHandles[ADCS_APP_PLATFORM_NUMBER_OF_TABLES];
} ADCS_APP_Data_t;

/*
** Global data structure
*/
extern ADCS_APP_Data_t ADCS_APP_Data;

/****************************************************************************/
/*
** Local function prototypes.
**
** Note: Except for the entry point (ADCS_APP_Main), these
**       functions are not called from any other source module.
*/
void         ADCS_APP_Main(void);
CFE_Status_t ADCS_APP_Init(void);

#endif /* ADCS_APP_H */
