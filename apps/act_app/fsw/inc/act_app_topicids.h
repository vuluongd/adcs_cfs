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
 *   ACT_APP Application Topic IDs
 */
#ifndef ACT_APP_TOPICIDS_H
#define ACT_APP_TOPICIDS_H

#include "act_app_topicid_values.h"

#define ACT_APP_MISSION_CMD_TOPICID             ACT_APP_MISSION_TIDVAL(CMD)
#define DEFAULT_ACT_APP_MISSION_CMD_TOPICID     0xE0
#define ACT_APP_MISSION_SEND_HK_TOPICID         ACT_APP_MISSION_TIDVAL(SEND_HK)
#define DEFAULT_ACT_APP_MISSION_SEND_HK_TOPICID 0xE1
#define ACT_APP_MISSION_HK_TLM_TOPICID          ACT_APP_MISSION_TIDVAL(HK_TLM)
#define DEFAULT_ACT_APP_MISSION_HK_TLM_TOPICID  0xE2
#define ACT_APP_MISSION_WAKEUP_TOPICID          ACT_APP_MISSION_TIDVAL(WAKEUP)
#define DEFAULT_ACT_APP_MISSION_WAKEUP_TOPICID  0xE3 /* WAKEUP_ACT: from SCH_LAB, CMD MID 0x18E3 */
#define ACT_APP_MISSION_DATA_TOPICID            ACT_APP_MISSION_TIDVAL(DATA)
#define DEFAULT_ACT_APP_MISSION_DATA_TOPICID    0xE4 /* WHEEL_DATA: TLM MID 0x08E4 */

#endif
