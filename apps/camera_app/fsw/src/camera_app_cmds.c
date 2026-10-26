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
 *   This file contains the source code for the Camera App Ground Command-handling functions
 */

/*
** Include Files:
*/
#include "camera_app.h"
#include "camera_app_cmds.h"
#include "camera_app_msgids.h"
#include "camera_app_eventids.h"
#include "camera_app_version.h"
#include "camera_app_tbl.h"
#include "camera_app_utils.h"
#include "camera_app_msg.h"

/* The camera_lib module provides the CAMERA_Function() prototype */
#include "camera_lib.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function is triggered in response to a task telemetry request */
/*         from the housekeeping task. This function will gather the Apps     */
/*         telemetry, packetize it and send it to the housekeeping task via   */
/*         the software bus                                                   */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
CFE_Status_t CAMERA_APP_SendHkCmd(const CAMERA_APP_SendHkCmd_t *Msg)
{
    int i;

    /*
    ** Get command execution counters...
    */
    CAMERA_APP_Data.HkTlm.Payload.CommandErrorCounter = CAMERA_APP_Data.CommandErrorCounter;
    CAMERA_APP_Data.HkTlm.Payload.CommandCounter      = CAMERA_APP_Data.CommandCounter;

    /*
    ** Send housekeeping telemetry packet...
    */
    CFE_SB_TimeStampMsg(CFE_MSG_PTR(CAMERA_APP_Data.HkTlm.TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(CAMERA_APP_Data.HkTlm.TelemetryHeader), true);

    /*
    ** Manage any pending table loads, validations, etc.
    */
    for (i = 0; i < CAMERA_APP_PLATFORM_NUMBER_OF_TABLES; i++)
    {
        CFE_TBL_Manage(CAMERA_APP_Data.TblHandles[i]);
    }

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* CAMERA NOOP commands                                                       */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
CFE_Status_t CAMERA_APP_NoopCmd(const CAMERA_APP_NoopCmd_t *Msg)
{
    CAMERA_APP_Data.CommandCounter++;

    CFE_EVS_SendEvent(CAMERA_APP_NOOP_INF_EID,
                      CFE_EVS_EventType_INFORMATION,
                      "CAMERA: NOOP command %s",
                      CAMERA_APP_VERSION);

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function resets all the global counter variables that are     */
/*         part of the task telemetry.                                        */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
CFE_Status_t CAMERA_APP_ResetCountersCmd(const CAMERA_APP_ResetCountersCmd_t *Msg)
{
    CAMERA_APP_Data.CommandCounter      = 0;
    CAMERA_APP_Data.CommandErrorCounter = 0;

    CFE_EVS_SendEvent(CAMERA_APP_RESET_INF_EID, CFE_EVS_EventType_INFORMATION, "CAMERA: RESET command");

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function Process Ground Station Command                       */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
CFE_Status_t CAMERA_APP_ProcessCmd(const CAMERA_APP_ProcessCmd_t *Msg)
{
    CFE_Status_t               Status;
    void                      *TblAddr;
    CAMERA_APP_ExampleTable_t *TblPtr;
    const char                *TableName = "CAMERA_APP.ExampleTable";

    /* Camera Use of Example Table */
    CAMERA_APP_Data.CommandCounter++;
    Status = CFE_TBL_GetAddress(&TblAddr, CAMERA_APP_Data.TblHandles[0]);
    if (Status < CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Camera App: Fail to get table address: 0x%08lx", (unsigned long)Status);
    }
    else
    {
        TblPtr = TblAddr;
        CFE_ES_WriteToSysLog("Camera App: Example Table Value 1: %d  Value 2: %d", TblPtr->Int1, TblPtr->Int2);

        CAMERA_APP_GetCrc(TableName);

        Status = CFE_TBL_ReleaseAddress(CAMERA_APP_Data.TblHandles[0]);
        if (Status != CFE_SUCCESS)
        {
            CFE_ES_WriteToSysLog("Camera App: Fail to release table address: 0x%08lx", (unsigned long)Status);
        }
        else
        {
            /* Invoke a function provided by CAMERA_APP_LIB */
            CAMERA_LIB_Function();
        }
    }

    return Status;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* A simple example command that displays a passed-in value                   */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
CFE_Status_t CAMERA_APP_DisplayParamCmd(const CAMERA_APP_DisplayParamCmd_t *Msg)
{
    CAMERA_APP_Data.CommandCounter++;
    CFE_EVS_SendEvent(CAMERA_APP_VALUE_INF_EID,
                      CFE_EVS_EventType_INFORMATION,
                      "CAMERA_APP: ValU32=%lu, ValI16=%d, ValStr=%s",
                      (unsigned long)Msg->Payload.ValU32,
                      (int)Msg->Payload.ValI16,
                      Msg->Payload.ValStr);

    return CFE_SUCCESS;
}
