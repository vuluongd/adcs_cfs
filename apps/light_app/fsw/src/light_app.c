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
#include "light_app.h"
#include "light_app_cmds.h"
#include "light_app_utils.h"
#include "light_app_eventids.h"
#include "light_app_dispatch.h"
#include "light_app_tbl.h"
#include "light_app_version.h"

/*
** global data
*/
LIGHT_APP_Data_t LIGHT_APP_Data;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Application entry point and main process loop                              */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
void LIGHT_APP_Main(void)
{
    CFE_Status_t     status;
    CFE_SB_Buffer_t *SBBufPtr;

    /*
    ** Create the first Performance Log entry
    */
    CFE_ES_PerfLogEntry(LIGHT_APP_PERF_ID);

    /*
    ** Perform application-specific initialization
    ** If the Initialization fails, set the RunStatus to
    ** CFE_ES_RunStatus_APP_ERROR and the App will not enter the RunLoop
    */
    status = LIGHT_APP_Init();
    if (status != CFE_SUCCESS)
    {
        LIGHT_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    /*
    ** Sample App Runloop
    */
    while (CFE_ES_RunLoop(&LIGHT_APP_Data.RunStatus) == true)
    {
        /*
        ** Performance Log Exit Stamp
        */
        CFE_ES_PerfLogExit(LIGHT_APP_PERF_ID);

        /* Pend on receipt of command packet */
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, LIGHT_APP_Data.CommandPipe, CFE_SB_PEND_FOREVER);

        /*
        ** Performance Log Entry Stamp
        */
        CFE_ES_PerfLogEntry(LIGHT_APP_PERF_ID);

        if (status == CFE_SUCCESS)
        {
            LIGHT_APP_TaskPipe(SBBufPtr);
        }
        else
        {
            CFE_EVS_SendEvent(LIGHT_APP_PIPE_ERR_EID,
                              CFE_EVS_EventType_ERROR,
                              "SAMPLE APP: SB Pipe Read Error, App Will Exit");

            LIGHT_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    /*
    ** Performance Log Exit Stamp
    */
    CFE_ES_PerfLogExit(LIGHT_APP_PERF_ID);

    CFE_ES_ExitApp(LIGHT_APP_Data.RunStatus);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  */
/*                                                                            */
/* Initialization                                                             */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
CFE_Status_t LIGHT_APP_Init(void)
{
    CFE_Status_t status;
    char         VersionString[LIGHT_APP_CFG_MAX_VERSION_STR_LEN];

    /* Zero out the global data structure */
    memset(&LIGHT_APP_Data, 0, sizeof(LIGHT_APP_Data));

    LIGHT_APP_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

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
        CFE_MSG_Init(CFE_MSG_PTR(LIGHT_APP_Data.HkTlm.TelemetryHeader),
                     CFE_SB_ValueToMsgId(LIGHT_APP_HK_TLM_MID),
                     sizeof(LIGHT_APP_Data.HkTlm));

        /*
         ** Create Software Bus message pipe.
         */
        status = CFE_SB_CreatePipe(&LIGHT_APP_Data.CommandPipe,
                                   LIGHT_APP_PLATFORM_PIPE_DEPTH,
                                   LIGHT_APP_PLATFORM_PIPE_NAME);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(LIGHT_APP_CR_PIPE_ERR_EID,
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
        status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LIGHT_APP_SEND_HK_MID), LIGHT_APP_Data.CommandPipe);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(LIGHT_APP_SUB_HK_ERR_EID,
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
        status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(LIGHT_APP_CMD_MID), LIGHT_APP_Data.CommandPipe);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(LIGHT_APP_SUB_CMD_ERR_EID,
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
        status = CFE_TBL_Register(&LIGHT_APP_Data.TblHandles[0],
                                  "ExampleTable",
                                  sizeof(LIGHT_APP_ExampleTable_t),
                                  CFE_TBL_OPT_DEFAULT,
                                  LIGHT_APP_TblValidationFunc);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(LIGHT_APP_TABLE_REG_ERR_EID,
                              CFE_EVS_EventType_ERROR,
                              "Sample App: Error Registering Example Table, RC = 0x%08lX",
                              (unsigned long)status);
        }
        else
        {
            status = CFE_TBL_Load(LIGHT_APP_Data.TblHandles[0], CFE_TBL_SRC_FILE, LIGHT_APP_PLATFORM_TABLE_FILE);
        }

        CFE_Config_GetVersionString(VersionString,
                                    LIGHT_APP_CFG_MAX_VERSION_STR_LEN,
                                    "Sample App",
                                    LIGHT_APP_VERSION,
                                    LIGHT_APP_BUILD_CODENAME,
                                    LIGHT_APP_LAST_OFFICIAL);

        CFE_EVS_SendEvent(LIGHT_APP_INIT_INF_EID,
                          CFE_EVS_EventType_INFORMATION,
                          "Sample App Initialized.%s",
                          VersionString);
    }

    return status;
}
