#ifndef ADCS_APP_CONTROL_H
#define ADCS_APP_CONTROL_H

/*
 * Luật điều khiển vòng ngoài (P) của ADCS_APP. Không phụ thuộc cFE nên test được trên PC.
 *
 *   light_app : cho góc lệch tới nguồn sáng (bearing, dương = ánh sáng nằm bên PHẢI thân)
 *   imu_app   : cho góc tích phân (angle_rad) -> "nội suy" góc còn lại giữa 2 mẫu ánh sáng
 *
 *   err   = góc còn phải quay tới ánh sáng, dương = phải quay sang phải   [rad]
 *   omega = wheel_sign * Kp * deadband(err)  rồi kẹp ±omega_limit          [rad/s]
 *
 * omega được gửi sang ACT_APP (SET_WHEEL_SPEED), vòng trong PI tốc độ nằm ở ACT_APP.
 */

#include <stdbool.h>
#include <stdint.h>

/* Giá trị mặc định - CẦN KIỂM TRA / TINH CHỈNH TRÊN PHẦN CỨNG */
#define ADCS_CTRL_DEFAULT_KP          100.0f /* rad/s tốc độ bánh đà trên mỗi rad sai số   */
#define ADCS_CTRL_DEFAULT_LIMIT       200.0f /* rad/s, phải <= omega_max của ACT_APP (300) */
#define ADCS_CTRL_DEFAULT_DEADBAND    0.035f /* rad (~2 độ)                                */
#define ADCS_CTRL_DEFAULT_HOLD_S      2.0f   /* s giữ mục tiêu bằng IMU khi mất ánh sáng   */
#define ADCS_CTRL_ABS_MAX_LIMIT       300.0f /* trần cứng, = omega_max mặc định ACT_APP    */

/* Dấu quy ước - KIỂM TRA BẰNG THỰC NGHIỆM (xem README bên dưới)                           */
/* wheel_sign: +1 nghĩa là sai số dương (ánh sáng bên phải) -> bánh đà quay dương (CCW)    */
/*             (động lượng bảo toàn: bánh CCW => thân quay CW = sang phải).                */
/* imu_right_sign: dấu của độ biến thiên IMU angle_rad khi thân quay sang phải.            */
/*             Gyro Z chuẩn (CCW dương) => quay phải làm angle GIẢM => -1.                 */
#define ADCS_CTRL_DEFAULT_WHEEL_SIGN     1.0f
#define ADCS_CTRL_DEFAULT_IMU_RIGHT_SIGN (-1.0f)

typedef enum
{
    ADCS_CTRL_IDLE        = 0, /* chưa có mục tiêu / bị tắt                          */
    ADCS_CTRL_LIGHT       = 1, /* đang dùng mẫu ánh sáng mới                         */
    ADCS_CTRL_IMU_HOLD    = 2, /* mất mẫu ánh sáng mới, tiếp tục theo IMU            */
    ADCS_CTRL_LOST        = 3  /* không còn nguồn tin cậy -> omega = 0               */
} ADCS_Ctrl_State_t;

typedef struct
{
    float kp;
    float omega_limit_rad_s;
    float deadband_rad;
    float hold_s;
    float wheel_sign;     /* +1 / -1 */
    float imu_right_sign; /* +1 / -1 */
} ADCS_Ctrl_Params_t;

typedef struct
{
    bool     have_target;
    bool     imu_cap_valid;
    float    bearing_cap_rad; /* bearing lúc lấy mẫu ánh sáng gần nhất                  */
    float    imu_cap_rad;     /* IMU angle tại thời điểm đó                             */
    float    since_light_s;
    uint32_t last_light_counter;
} ADCS_Ctrl_Mem_t;

typedef struct
{
    bool     light_usable;   /* ADCS_APP_LightUsable(): bearing hợp lệ & còn mới       */
    uint32_t light_counter;  /* sample_counter của LIGHT_DATA                          */
    float    bearing_rad;
    bool     imu_usable;     /* ADCS_APP_ImuUsable()                                   */
    float    imu_angle_rad;
} ADCS_Ctrl_In_t;

typedef struct
{
    float             error_rad;
    float             omega_cmd_rad_s;
    ADCS_Ctrl_State_t state;
} ADCS_Ctrl_Out_t;

void ADCS_Ctrl_DefaultParams(ADCS_Ctrl_Params_t *p);
bool ADCS_Ctrl_ParamsValid(const ADCS_Ctrl_Params_t *p);
void ADCS_Ctrl_Reset(ADCS_Ctrl_Mem_t *m);
void ADCS_Ctrl_Step(const ADCS_Ctrl_Params_t *p, ADCS_Ctrl_Mem_t *m, const ADCS_Ctrl_In_t *in, float dt_s,
                    ADCS_Ctrl_Out_t *out);

#endif
