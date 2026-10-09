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
 *   This file contains the source code for the Sample App Ground Command-handling functions
 */

/*
** Include Files:
*/
#include "light_app.h"
#include "light_app_cmds.h"
#include "light_app_msgids.h"
#include "light_app_eventids.h"
#include "light_app_version.h"
#include "light_app_tbl.h"
#include "light_app_utils.h"
#include "light_app_msg.h"

/* The sample_lib module provides the SAMPLE_Function() prototype */
#include "sample_lib.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function is triggered in response to a task telemetry request */
/*         from the housekeeping task. This function will gather the Apps     */
/*         telemetry, packetize it and send it to the housekeeping task via   */
/*         the software bus                                                   */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
CFE_Status_t LIGHT_APP_SendHkCmd(const LIGHT_APP_SendHkCmd_t *Msg)
{
    int i;

    /*
    ** Get command execution counters...
    */
    LIGHT_APP_Data.HkTlm.Payload.CommandErrorCounter = LIGHT_APP_Data.CommandErrorCounter;
    LIGHT_APP_Data.HkTlm.Payload.CommandCounter      = LIGHT_APP_Data.CommandCounter;

    /*
    ** Send housekeeping telemetry packet...
    */
    CFE_SB_TimeStampMsg(CFE_MSG_PTR(LIGHT_APP_Data.HkTlm.TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(LIGHT_APP_Data.HkTlm.TelemetryHeader), true);

    /*
    ** Manage any pending table loads, validations, etc.
    */
    for (i = 0; i < LIGHT_APP_PLATFORM_NUMBER_OF_TABLES; i++)
    {
        CFE_TBL_Manage(LIGHT_APP_Data.TblHandles[i]);
    }

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* SAMPLE NOOP commands                                                       */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
CFE_Status_t LIGHT_APP_NoopCmd(const LIGHT_APP_NoopCmd_t *Msg)
{
    LIGHT_APP_Data.CommandCounter++;

    CFE_EVS_SendEvent(LIGHT_APP_NOOP_INF_EID,
                      CFE_EVS_EventType_INFORMATION,
                      "SAMPLE: NOOP command %s",
                      LIGHT_APP_VERSION);

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function resets all the global counter variables that are     */
/*         part of the task telemetry.                                        */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
CFE_Status_t LIGHT_APP_ResetCountersCmd(const LIGHT_APP_ResetCountersCmd_t *Msg)
{
    LIGHT_APP_Data.CommandCounter      = 0;
    LIGHT_APP_Data.CommandErrorCounter = 0;

    CFE_EVS_SendEvent(LIGHT_APP_RESET_INF_EID, CFE_EVS_EventType_INFORMATION, "SAMPLE: RESET command");

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function Process Ground Station Command                       */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
CFE_Status_t LIGHT_APP_ProcessCmd(const LIGHT_APP_ProcessCmd_t *Msg)
{
    CFE_Status_t               Status;
    void                      *TblAddr;
    LIGHT_APP_ExampleTable_t *TblPtr;
    const char                *TableName = "LIGHT_APP.ExampleTable";

    /* Sample Use of Example Table */
    LIGHT_APP_Data.CommandCounter++;
    Status = CFE_TBL_GetAddress(&TblAddr, LIGHT_APP_Data.TblHandles[0]);
    if (Status < CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Sample App: Fail to get table address: 0x%08lx", (unsigned long)Status);
    }
    else
    {
        TblPtr = TblAddr;
        CFE_ES_WriteToSysLog("Sample App: Example Table Value 1: %d  Value 2: %d", TblPtr->Int1, TblPtr->Int2);

        LIGHT_APP_GetCrc(TableName);

        Status = CFE_TBL_ReleaseAddress(LIGHT_APP_Data.TblHandles[0]);
        if (Status != CFE_SUCCESS)
        {
            CFE_ES_WriteToSysLog("Sample App: Fail to release table address: 0x%08lx", (unsigned long)Status);
        }
        else
        {
            /* Invoke a function provided by LIGHT_APP_LIB */
            SAMPLE_LIB_Function();
        }
    }

    return Status;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* A simple example command that displays a passed-in value                   */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
CFE_Status_t LIGHT_APP_DisplayParamCmd(const LIGHT_APP_DisplayParamCmd_t *Msg)
{
    LIGHT_APP_Data.CommandCounter++;
    CFE_EVS_SendEvent(LIGHT_APP_VALUE_INF_EID,
                      CFE_EVS_EventType_INFORMATION,
                      "LIGHT_APP: ValU32=%lu, ValI16=%d, ValStr=%s",
                      (unsigned long)Msg->Payload.ValU32,
                      (int)Msg->Payload.ValI16,
                      Msg->Payload.ValStr);

    return CFE_SUCCESS;
}
