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
#include "adcs_app_dispatch.h"
#include "adcs_app_cmds.h"
#include "adcs_app_eventids.h"
#include "adcs_app_msgids.h"
#include "adcs_app_msg.h"
#include "light_app_msgids.h"
#include "adcs_app_light.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* Verify command packet length                                               */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
bool ADCS_APP_VerifyCmdLength(const CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength)
{
    bool              result       = true;
    size_t            ActualLength = 0;
    CFE_SB_MsgId_t    MsgId        = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t FcnCode      = 0;

    CFE_MSG_GetSize(MsgPtr, &ActualLength);

    /*
    ** Verify the command packet length.
    */
    if (ExpectedLength != ActualLength)
    {
        CFE_MSG_GetMsgId(MsgPtr, &MsgId);
        CFE_MSG_GetFcnCode(MsgPtr, &FcnCode);

        CFE_EVS_SendEvent(ADCS_APP_CMD_LEN_ERR_EID,
                          CFE_EVS_EventType_ERROR,
                          "Invalid Msg length: ID = 0x%X,  CC = %u, Len = %u, Expected = %u",
                          (unsigned int)CFE_SB_MsgIdToValue(MsgId),
                          (unsigned int)FcnCode,
                          (unsigned int)ActualLength,
                          (unsigned int)ExpectedLength);

        result = false;

        ADCS_APP_Data.CommandErrorCounter++;
    }

    return result;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* SAMPLE ground commands                                                     */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
void ADCS_APP_ProcessGroundCommand(const CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_MSG_FcnCode_t CommandCode = 0;

    CFE_MSG_GetFcnCode(&SBBufPtr->Msg, &CommandCode);

    /*
    ** Process SAMPLE app ground commands
    */
    switch (CommandCode)
    {
        case ADCS_APP_NOOP_CC:
            if (ADCS_APP_VerifyCmdLength(&SBBufPtr->Msg, sizeof(ADCS_APP_NoopCmd_t)))
            {
                ADCS_APP_NoopCmd((const ADCS_APP_NoopCmd_t *)SBBufPtr);
            }
            break;

        case ADCS_APP_RESET_COUNTERS_CC:
            if (ADCS_APP_VerifyCmdLength(&SBBufPtr->Msg, sizeof(ADCS_APP_ResetCountersCmd_t)))
            {
                ADCS_APP_ResetCountersCmd((const ADCS_APP_ResetCountersCmd_t *)SBBufPtr);
            }
            break;

        case ADCS_APP_PROCESS_CC:
            if (ADCS_APP_VerifyCmdLength(&SBBufPtr->Msg, sizeof(ADCS_APP_ProcessCmd_t)))
            {
                ADCS_APP_ProcessCmd((const ADCS_APP_ProcessCmd_t *)SBBufPtr);
            }
            break;

        case ADCS_APP_DISPLAY_PARAM_CC:
            if (ADCS_APP_VerifyCmdLength(&SBBufPtr->Msg, sizeof(ADCS_APP_DisplayParamCmd_t)))
            {
                ADCS_APP_DisplayParamCmd((const ADCS_APP_DisplayParamCmd_t *)SBBufPtr);
            }
            break;

        default:
            CFE_EVS_SendEvent(ADCS_APP_CC_ERR_EID,
                              CFE_EVS_EventType_ERROR,
                              "Invalid ground command code: CC = %d",
                              CommandCode);
            break;
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*     This routine will process any packet that is received on the SAMPLE    */
/*     command pipe.                                                          */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
void ADCS_APP_TaskPipe(const CFE_SB_Buffer_t *SBBufPtr)
{
    static CFE_SB_MsgId_t CMD_MID     = CFE_SB_MSGID_RESERVED;
    static CFE_SB_MsgId_t SEND_HK_MID = CFE_SB_MSGID_RESERVED;

    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    /* cache the local MID Values here, this avoids repeat lookups */
    if (!CFE_SB_IsValidMsgId(CMD_MID))
    {
        CMD_MID     = CFE_SB_ValueToMsgId(ADCS_APP_CMD_MID);
        SEND_HK_MID = CFE_SB_ValueToMsgId(ADCS_APP_SEND_HK_MID);
    }

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    /* Process all SB messages */
    if (CFE_SB_MsgId_Equal(MsgId, CFE_SB_ValueToMsgId(LIGHT_APP_DATA_MID)))
    {
        ADCS_APP_ReceiveLight(SBBufPtr);
    }
    else if (CFE_SB_MsgId_Equal(MsgId, SEND_HK_MID))
    {
        /* Housekeeping request */
        ADCS_APP_SendHkCmd((const ADCS_APP_SendHkCmd_t *)SBBufPtr);
    }
    else if (CFE_SB_MsgId_Equal(MsgId, CMD_MID))
    {
        /* Ground command */
        ADCS_APP_ProcessGroundCommand(SBBufPtr);
    }
    else
    {
        /* Unknown command */
        CFE_EVS_SendEvent(ADCS_APP_MID_ERR_EID,
                          CFE_EVS_EventType_ERROR,
                          "SAMPLE: invalid command packet,MID = 0x%x",
                          (unsigned int)CFE_SB_MsgIdToValue(MsgId));
    }
}
