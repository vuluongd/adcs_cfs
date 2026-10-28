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
 *   This file contains the source code for ACT_APP (reaction wheel actuator) Ground Command-handling functions
 */

/*
** Include Files:
*/
#include "act_app.h"
#include "act_app_cmds.h"
#include "act_app_msgids.h"
#include "act_app_eventids.h"
#include "act_app_version.h"
#include "act_app_tbl.h"
#include "act_app_utils.h"
#include "act_app_msg.h"

/* The sample_lib module provides the SAMPLE_Function() prototype */
#include "sample_lib.h"
#include "act_app_wheel.h"
#include "act_status.h"
#include <math.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function is triggered in response to a task telemetry request */
/*         from the housekeeping task. This function will gather the Apps     */
/*         telemetry, packetize it and send it to the housekeeping task via   */
/*         the software bus                                                   */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
CFE_Status_t ACT_APP_SendHkCmd(const ACT_APP_SendHkCmd_t *Msg)
{
    int i;

    /*
    ** Get command execution counters...
    */
    ACT_APP_Data.HkTlm.Payload.CommandErrorCounter = ACT_APP_Data.CommandErrorCounter;
    ACT_APP_Data.HkTlm.Payload.CommandCounter      = ACT_APP_Data.CommandCounter;

    /*
    ** Send housekeeping telemetry packet...
    */
    CFE_SB_TimeStampMsg(CFE_MSG_PTR(ACT_APP_Data.HkTlm.TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(ACT_APP_Data.HkTlm.TelemetryHeader), true);

    /*
    ** Manage any pending table loads, validations, etc.
    */
    for (i = 0; i < ACT_APP_PLATFORM_NUMBER_OF_TABLES; i++)
    {
        CFE_TBL_Manage(ACT_APP_Data.TblHandles[i]);
    }

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* ACT_APP NOOP commands                                                       */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
CFE_Status_t ACT_APP_NoopCmd(const ACT_APP_NoopCmd_t *Msg)
{
    ACT_APP_Data.CommandCounter++;

    CFE_EVS_SendEvent(ACT_APP_NOOP_INF_EID,
                      CFE_EVS_EventType_INFORMATION,
                      "ACT_APP: NOOP command %s",
                      ACT_APP_VERSION);

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function resets all the global counter variables that are     */
/*         part of the task telemetry.                                        */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
CFE_Status_t ACT_APP_ResetCountersCmd(const ACT_APP_ResetCountersCmd_t *Msg)
{
    ACT_APP_Data.CommandCounter      = 0;
    ACT_APP_Data.CommandErrorCounter = 0;

    CFE_EVS_SendEvent(ACT_APP_RESET_INF_EID, CFE_EVS_EventType_INFORMATION, "ACT_APP: RESET command");

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function Process Ground Station Command                       */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
CFE_Status_t ACT_APP_ProcessCmd(const ACT_APP_ProcessCmd_t *Msg)
{
    CFE_Status_t               Status;
    void                      *TblAddr;
    ACT_APP_ExampleTable_t *TblPtr;
    const char                *TableName = "ACT_APP.ExampleTable";

    /* Example table demo (left from sample_app) */
    ACT_APP_Data.CommandCounter++;
    Status = CFE_TBL_GetAddress(&TblAddr, ACT_APP_Data.TblHandles[0]);
    if (Status < CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("ACT_APP: Fail to get table address: 0x%08lx", (unsigned long)Status);
    }
    else
    {
        TblPtr = TblAddr;
        CFE_ES_WriteToSysLog("ACT_APP: Example Table Value 1: %d  Value 2: %d", TblPtr->Int1, TblPtr->Int2);

        ACT_APP_GetCrc(TableName);

        Status = CFE_TBL_ReleaseAddress(ACT_APP_Data.TblHandles[0]);
        if (Status != CFE_SUCCESS)
        {
            CFE_ES_WriteToSysLog("ACT_APP: Fail to release table address: 0x%08lx", (unsigned long)Status);
        }
        else
        {
            /* Invoke a function provided by ACT_APP_LIB */
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
CFE_Status_t ACT_APP_DisplayParamCmd(const ACT_APP_DisplayParamCmd_t *Msg)
{
    ACT_APP_Data.CommandCounter++;
    CFE_EVS_SendEvent(ACT_APP_VALUE_INF_EID,
                      CFE_EVS_EventType_INFORMATION,
                      "ACT_APP: ValU32=%lu, ValI16=%d, ValStr=%s",
                      (unsigned long)Msg->Payload.ValU32,
                      (int)Msg->Payload.ValI16,
                      Msg->Payload.ValStr);

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*  ACT_CMD: wheel speed setpoint (rad/s, sign = direction)                   */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
CFE_Status_t ACT_APP_SetWheelSpeedCmd(const ACT_APP_SetWheelSpeedCmd_t *Msg)
{
    uint8 st;

    if (!isfinite(Msg->Payload.omega_rad_s))
    {
        ACT_APP_Data.CommandErrorCounter++;
        CFE_EVS_SendEvent(ACT_APP_CMD_LEN_ERR_EID, CFE_EVS_EventType_ERROR, "ACT_APP: SET_WHEEL_SPEED rejected, non-finite setpoint");
        return CFE_STATUS_RANGE_ERROR;
    }

    st = ACT_APP_WheelSetSpeed(Msg->Payload.omega_rad_s);
    ACT_APP_Data.CommandCounter++;
    CFE_EVS_SendEvent(ACT_APP_VALUE_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "ACT_APP: SET_WHEEL_SPEED %.2f rad/s%s", (double)Msg->Payload.omega_rad_s,
                      (st == ACT_STATUS_SATURATED) ? " (clamped to omega_max)" : "");
    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*  Ramp to zero, then brake and disable the motor.                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
CFE_Status_t ACT_APP_StopWheelCmd(const ACT_APP_StopWheelCmd_t *Msg)
{
    ACT_APP_WheelStop();
    ACT_APP_Data.CommandCounter++;
    CFE_EVS_SendEvent(ACT_APP_VALUE_INF_EID, CFE_EVS_EventType_INFORMATION, "ACT_APP: STOP_WHEEL");
    return CFE_SUCCESS;
}
