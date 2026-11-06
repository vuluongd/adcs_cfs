/* Host test: gcc -I fsw/inc tests/test_control.c fsw/src/adcs_app_control.c -lm -o /tmp/t && /tmp/t */
#include "adcs_app_control.h"
#include <math.h>
#include <stdio.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void)
{
    ADCS_Ctrl_Params_t p; ADCS_Ctrl_Mem_t m; ADCS_Ctrl_In_t in = {0}; ADCS_Ctrl_Out_t o;
    ADCS_Ctrl_DefaultParams(&p); CHECK(ADCS_Ctrl_ParamsValid(&p)); ADCS_Ctrl_Reset(&m);

    /* 1. chưa có ánh sáng -> 0 */
    ADCS_Ctrl_Step(&p, &m, &in, 0.1f, &o);
    CHECK(o.omega_cmd_rad_s == 0.0f && o.state == ADCS_CTRL_LOST);

    /* 2. ánh sáng bên phải 0.5 rad -> omega dương, đúng Kp*(e-db) */
    in.light_usable = true; in.light_counter = 1; in.bearing_rad = 0.5f; in.imu_usable = true; in.imu_angle_rad = 1.0f;
    ADCS_Ctrl_Step(&p, &m, &in, 0.1f, &o);
    CHECK(fabsf(o.omega_cmd_rad_s - p.kp * (0.5f - p.deadband_rad)) < 1e-3f && o.state == ADCS_CTRL_LIGHT);

    /* 3. mẫu cũ, thân quay phải 0.2 rad (imu giảm 0.2) -> err = 0.3 */
    in.imu_angle_rad = 0.8f;
    ADCS_Ctrl_Step(&p, &m, &in, 0.1f, &o);
    CHECK(fabsf(o.error_rad - 0.3f) < 1e-5f && o.state == ADCS_CTRL_IMU_HOLD);

    /* 4. ánh sáng bên trái -> omega âm */
    in.light_counter = 2; in.bearing_rad = -0.4f;
    ADCS_Ctrl_Step(&p, &m, &in, 0.1f, &o);
    CHECK(o.omega_cmd_rad_s < 0.0f);

    /* 5. vùng chết */
    in.light_counter = 3; in.bearing_rad = 0.02f;
    ADCS_Ctrl_Step(&p, &m, &in, 0.1f, &o);
    CHECK(o.omega_cmd_rad_s == 0.0f);

    /* 6. kẹp */
    {
        ADCS_Ctrl_Params_t hi = p; hi.kp = 300.0f;
        in.light_counter = 4; in.bearing_rad = 1.5f;
        ADCS_Ctrl_Step(&hi, &m, &in, 0.1f, &o);
        CHECK(o.omega_cmd_rad_s == hi.omega_limit_rad_s);
        in.light_counter = 5; in.bearing_rad = -1.5f;
        ADCS_Ctrl_Step(&hi, &m, &in, 0.1f, &o);
        CHECK(o.omega_cmd_rad_s == -hi.omega_limit_rad_s);
    }

    /* 7. mất ánh sáng + IMU còn: giữ tới hold_s rồi về 0 */
    in.light_usable = false;
    int held = 0;
    for (int i = 0; i < 40; i++) { ADCS_Ctrl_Step(&p, &m, &in, 0.1f, &o); if (o.state == ADCS_CTRL_IMU_HOLD) held++; }
    CHECK((held == 19 || held == 20) && o.omega_cmd_rad_s == 0.0f && o.state == ADCS_CTRL_LOST);

    /* 8. mất ánh sáng + mất IMU -> dừng ngay */
    ADCS_Ctrl_Reset(&m);
    in.light_usable = true; in.light_counter = 10; in.bearing_rad = 0.5f;
    ADCS_Ctrl_Step(&p, &m, &in, 0.1f, &o);
    in.light_usable = false; in.imu_usable = false;
    ADCS_Ctrl_Step(&p, &m, &in, 0.1f, &o);
    CHECK(o.omega_cmd_rad_s == 0.0f && o.state == ADCS_CTRL_LOST);

    /* 9. chỉ có ánh sáng (IMU hỏng): vẫn chạy P theo bearing */
    ADCS_Ctrl_Reset(&m);
    in.light_usable = true; in.light_counter = 11; in.imu_usable = false;
    ADCS_Ctrl_Step(&p, &m, &in, 0.1f, &o);
    CHECK(o.omega_cmd_rad_s > 0.0f);
    ADCS_Ctrl_Step(&p, &m, &in, 0.1f, &o); /* cùng mẫu, không được nhấp nháy về 0 */
    CHECK(o.omega_cmd_rad_s > 0.0f && o.state == ADCS_CTRL_LIGHT);

    /* 10. mô phỏng vòng kín: thân + bánh đà bảo toàn động lượng, bánh bám lệnh bậc 1 */
    {
        const float Ib = 0.02f, Iw = 0.002f, dt = 0.1f, tau = 0.5f;
        float theta_light = 0.8f, theta = 0.0f, w_body = 0.0f, w_wheel = 0.0f, imu = 0.0f;
        uint32_t n = 0; ADCS_Ctrl_Reset(&m);
        for (int k = 0; k < 600; k++)
        {
            ADCS_Ctrl_In_t i2 = {0};
            i2.imu_usable = true; i2.imu_angle_rad = imu;
            if (k % 2 == 0) { i2.light_usable = true; i2.light_counter = ++n; i2.bearing_rad = theta_light - theta; } /* 5 Hz */
            else { i2.light_usable = true; i2.light_counter = n; i2.bearing_rad = theta_light - theta; }
            ADCS_Ctrl_Step(&p, &m, &i2, dt, &o);
            float w_new = w_wheel + (o.omega_cmd_rad_s - w_wheel) * (dt / tau);
            w_body -= (Iw / Ib) * (w_new - w_wheel);   /* bảo toàn động lượng: bánh + => thân quay ngược (phải) */
            w_wheel = w_new;
            theta += -w_body * dt;                     /* w_body âm = quay phải = theta (phải dương) tăng */
            imu   += p.imu_right_sign * (-w_body * dt);
        }
        printf("closed-loop: residual error %.4f rad (deadband %.3f)\n", (double)(theta_light - theta), (double)p.deadband_rad);
        CHECK(fabsf(theta_light - theta) <= p.deadband_rad + 0.02f);
    }
    printf(fails ? "%d FAILED\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
