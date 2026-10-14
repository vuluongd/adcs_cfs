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
 * Auto-Generated stub implementations for functions defined in light_app header
 */

#include "light_app.h"
#include "utgenstub.h"
#include "light_app_tracking.h"

/*
 * ----------------------------------------------------
 * Generated stub function for LIGHT_APP_Init()
 * ----------------------------------------------------
 */
CFE_Status_t LIGHT_APP_Init(void)
{
    UT_GenStub_SetupReturnBuffer(LIGHT_APP_Init, CFE_Status_t);

    UT_GenStub_Execute(LIGHT_APP_Init, Basic, NULL);

    return UT_GenStub_GetReturnValue(LIGHT_APP_Init, CFE_Status_t);
}

/*
 * ----------------------------------------------------
 * Generated stub function for LIGHT_APP_Main()
 * ----------------------------------------------------
 */
void LIGHT_APP_Main(void)
{
    UT_GenStub_Execute(LIGHT_APP_Main, Basic, NULL);
}

CFE_Status_t LIGHT_APP_TrackingInit(void)
{
    UT_GenStub_SetupReturnBuffer(LIGHT_APP_TrackingInit, CFE_Status_t);
    UT_GenStub_Execute(LIGHT_APP_TrackingInit, Basic, NULL);
    return UT_GenStub_GetReturnValue(LIGHT_APP_TrackingInit, CFE_Status_t);
}
void LIGHT_APP_OnWakeup(void)
{
    UT_GenStub_Execute(LIGHT_APP_OnWakeup, Basic, NULL);
}
void LIGHT_APP_TrackingClose(void)
{
    UT_GenStub_Execute(LIGHT_APP_TrackingClose, Basic, NULL);
}
