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
 * \file
 *   This file contains the source code for the Sample App.
 */

/*
** Include Files:
*/
#include "adcs_app.h"
#include "adcs_app_cmds.h"
#include "adcs_app_utils.h"
#include "adcs_app_eventids.h"
#include "adcs_app_dispatch.h"
#include "adcs_app_tbl.h"
#include "adcs_app_version.h"
#include "light_app_msgids.h"

/*
** global data
*/
ADCS_APP_Data_t ADCS_APP_Data;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Application entry point and main process loop                              */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
void ADCS_APP_Main(void)
{
    CFE_Status_t     status;
    CFE_SB_Buffer_t *SBBufPtr;

    /*
    ** Create the first Performance Log entry
    */
    CFE_ES_PerfLogEntry(ADCS_APP_PERF_ID);

    /*
    ** Perform application-specific initialization
    ** If the Initialization fails, set the RunStatus to
    ** CFE_ES_RunStatus_APP_ERROR and the App will not enter the RunLoop
    */
    status = ADCS_APP_Init();
    if (status != CFE_SUCCESS)
    {
        ADCS_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    /*
    ** Sample App Runloop
    */
    while (CFE_ES_RunLoop(&ADCS_APP_Data.RunStatus) == true)
    {
        /*
        ** Performance Log Exit Stamp
        */
        CFE_ES_PerfLogExit(ADCS_APP_PERF_ID);

        /* Pend on receipt of command packet */
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, ADCS_APP_Data.CommandPipe, CFE_SB_PEND_FOREVER);

        /*
        ** Performance Log Entry Stamp
        */
        CFE_ES_PerfLogEntry(ADCS_APP_PERF_ID);

        if (status == CFE_SUCCESS)
        {
            ADCS_APP_TaskPipe(SBBufPtr);
        }
        else
        {
            CFE_EVS_SendEvent(ADCS_APP_PIPE_ERR_EID,
                              CFE_EVS_EventType_ERROR,
                              "SAMPLE APP: SB Pipe Read Error, App Will Exit");

            ADCS_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    /*
    ** Performance Log Exit Stamp
    */
    CFE_ES_PerfLogExit(ADCS_APP_PERF_ID);

    CFE_ES_ExitApp(ADCS_APP_Data.RunStatus);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  */
/*                                                                            */
/* Initialization                                                             */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
CFE_Status_t ADCS_APP_Init(void)
{
    CFE_Status_t status;
    char         VersionString[ADCS_APP_CFG_MAX_VERSION_STR_LEN];

    /* Zero out the global data structure */
    memset(&ADCS_APP_Data, 0, sizeof(ADCS_APP_Data));

    ADCS_APP_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    /*
    ** Register the events
    */
    status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Sample App: Error Registering Events, RC = 0x%08lX\n", (unsigned long)status);
    }
    else
    {
        /*
         ** Initialize housekeeping packet (clear user data area).
         */
        CFE_MSG_Init(CFE_MSG_PTR(ADCS_APP_Data.HkTlm.TelemetryHeader),
                     CFE_SB_ValueToMsgId(ADCS_APP_HK_TLM_MID),
                     sizeof(ADCS_APP_Data.HkTlm));

        /*
         ** Create Software Bus message pipe.
         */
        status = CFE_SB_CreatePipe(&ADCS_APP_Data.CommandPipe,
                                   ADCS_APP_PLATFORM_PIPE_DEPTH,
                                   ADCS_APP_PLATFORM_PIPE_NAME);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(ADCS_APP_CR_PIPE_ERR_EID,
                              CFE_EVS_EventType_ERROR,
                              "Sample App: Error creating SB Command Pipe, RC = 0x%08lX",
                              (unsigned long)status);
        }
    }

    if (status == CFE_SUCCESS)
    {
        /*
        ** Subscribe to Housekeeping request commands
        */
        status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ADCS_APP_SEND_HK_MID), ADCS_APP_Data.CommandPipe);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(ADCS_APP_SUB_HK_ERR_EID,
                              CFE_EVS_EventType_ERROR,
                              "Sample App: Error Subscribing to HK request, RC = 0x%08lX",
                              (unsigned long)status);
        }
    }

    if (status == CFE_SUCCESS)
    {
        /*
        ** Subscribe to ground command packets
        */
        status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ADCS_APP_CMD_MID), ADCS_APP_Data.CommandPipe);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(ADCS_APP_SUB_CMD_ERR_EID,
                              CFE_EVS_EventType_ERROR,
                              "Sample App: Error Subscribing to Commands, RC = 0x%08lX",
                              (unsigned long)status);
        }
    }

    if (status == CFE_SUCCESS)
    {
        /*
        ** Register Example Table(s)
        */
        status = CFE_TBL_Register(&ADCS_APP_Data.TblHandles[0],
                                  "ExampleTable",
                                  sizeof(ADCS_APP_ExampleTable_t),
                                  CFE_TBL_OPT_DEFAULT,
                                  ADCS_APP_TblValidationFunc);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(ADCS_APP_TABLE_REG_ERR_EID,
                              CFE_EVS_EventType_ERROR,
                              "Sample App: Error Registering Example Table, RC = 0x%08lX",
                              (unsigned long)status);
        }
        else
        {
            status = CFE_TBL_Load(ADCS_APP_Data.TblHandles[0], CFE_TBL_SRC_FILE, ADCS_APP_PLATFORM_TABLE_FILE);
        }

        CFE_Config_GetVersionString(VersionString,
                                    ADCS_APP_CFG_MAX_VERSION_STR_LEN,
                                    "Sample App",
                                    ADCS_APP_VERSION,
                                    ADCS_APP_BUILD_CODENAME,
                                    ADCS_APP_LAST_OFFICIAL);

        CFE_EVS_SendEvent(ADCS_APP_INIT_INF_EID,
                          CFE_EVS_EventType_INFORMATION,
                          "Sample App Initialized.%s",
                          VersionString);
    }

    if (status == CFE_SUCCESS)
    {
        status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LIGHT_APP_DATA_MID), ADCS_APP_Data.CommandPipe);
    }
    return status;
}
