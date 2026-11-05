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
 *   Specification for the ADCS_APP command and telemetry
 *   message payload and constant definitions.
 */
#ifndef DEFAULT_ADCS_APP_MSGDEFS_H
#define DEFAULT_ADCS_APP_MSGDEFS_H

#include "common_types.h"
#include "adcs_app_fcncodes.h"

typedef struct ADCS_APP_DisplayParam_Payload
{
    uint32 ValU32;                                    /**< 32 bit unsigned integer value */
    int16  ValI16;                                    /**< 16 bit signed integer value */
    char   ValStr[ADCS_APP_MISSION_STRING_VAL_LEN]; /**< An example string */
} ADCS_APP_DisplayParam_Payload_t;

/*************************************************************************/
/*
** SET_MODE: 0 = DISABLED (không gửi lệnh bánh đà, bánh đà được dừng), 1 = TRACKING (bám ánh sáng)
*/
typedef struct ADCS_APP_SetMode_Payload
{
    uint32 Mode;
} ADCS_APP_SetMode_Payload_t;

/*
** SET_PARAMS: tham số vòng ngoài P (kiểm tra hợp lệ trước khi áp dụng)
*/
typedef struct ADCS_APP_SetParams_Payload
{
    float  Kp;              /**< rad/s bánh đà trên mỗi rad sai số            */
    float  OmegaLimit;      /**< rad/s, 0 < giá trị <= 300                    */
    float  DeadbandRad;     /**< rad, vùng chết quanh góc 0                   */
    uint32 AllowSimulated;  /**< 1 = chấp nhận dữ liệu IMU/Light mô phỏng     */
} ADCS_APP_SetParams_Payload_t;

/*
** Housekeeping
*/
typedef struct ADCS_APP_HkTlm_Payload
{
    uint8 CommandCounter;
    uint8 CommandErrorCounter;
    uint8 Mode;            /**< 0 DISABLED, 1 TRACKING                         */
    uint8 CtrlState;       /**< ADCS_Ctrl_State_t: 0 IDLE 1 LIGHT 2 IMU_HOLD 3 LOST */
    float ErrorRad;        /**< sai số góc, dương = ánh sáng bên phải          */
    float OmegaCmdRadS;    /**< tốc độ bánh đà đã gửi sang ACT_APP             */
    float WheelMeasRadS;   /**< tốc độ bánh đà đo được (WHEEL_DATA)            */
    float Kp;
    float OmegaLimit;
    float DeadbandRad;
    uint8 ImuUsable;
    uint8 LightUsable;
    uint8 WheelValid;
    uint8 AllowSimulated;
} ADCS_APP_HkTlm_Payload_t;

#endif
