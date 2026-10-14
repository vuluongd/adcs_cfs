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
 *   LIGHT_APP Application Topic IDs
 */
#ifndef LIGHT_APP_TOPICIDS_H
#define LIGHT_APP_TOPICIDS_H

#include "light_app_topicid_values.h"

#define LIGHT_APP_MISSION_CMD_TOPICID             LIGHT_APP_MISSION_TIDVAL(CMD)
#define DEFAULT_LIGHT_APP_MISSION_CMD_TOPICID     0xD8
#define LIGHT_APP_MISSION_SEND_HK_TOPICID         LIGHT_APP_MISSION_TIDVAL(SEND_HK)
#define DEFAULT_LIGHT_APP_MISSION_SEND_HK_TOPICID 0xD9
#define LIGHT_APP_MISSION_HK_TLM_TOPICID          LIGHT_APP_MISSION_TIDVAL(HK_TLM)
#define DEFAULT_LIGHT_APP_MISSION_HK_TLM_TOPICID  0xDA
#define LIGHT_APP_MISSION_WAKEUP_TOPICID          LIGHT_APP_MISSION_TIDVAL(WAKEUP)
#define DEFAULT_LIGHT_APP_MISSION_WAKEUP_TOPICID  0xDB
#define LIGHT_APP_MISSION_DATA_TOPICID            LIGHT_APP_MISSION_TIDVAL(DATA)
#define DEFAULT_LIGHT_APP_MISSION_DATA_TOPICID    0xDC

#endif
