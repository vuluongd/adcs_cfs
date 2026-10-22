# IMU_APP – MPU6050 gyro Z, hiệu chuẩn bias, publish IMU_DATA

## Luồng
`SCH_LAB --IMU_APP_WAKEUP_MID (10 Hz)--> IMU_APP --IMU_DATA (MID 0x08D4)--> ADCS_APP / TO_LAB`

Mỗi WAKEUP: đọc MPU6050 (1 burst I²C 8 byte: nhiệt độ + gyro) → `dt` thật từ `CLOCK_MONOTONIC` → bộ lọc → publish.

## Bộ lọc (`imu_filter.c`, không phụ thuộc cFS)
1. Hiệu chuẩn bias: 10 s đầu, trung vị + MAD loại ngoại lai rồi lấy trung bình. Từ chối (và thử lại) nếu: std > 0,10 °/s, |bias| > 3 °/s, hai nửa cửa sổ lệch > 0,10 °/s, hoặc > 20 % ngoại lai.
2. Median 3 điểm.
3. Low-pass bậc 1, fc = 10 Hz, hệ số tính theo dt thật.
4. Đứng yên: |rate| < 0,15 °/s trong 1 s → đóng băng góc, bias thích nghi τ = 30 s (đáy Allan của log ở ~30 s).
5. Tích phân hình thang; khoảng mất mẫu (dt > 0,25 s) không tích phân.

## IMU_DATA (32 byte payload, SI)
| off | trường | ghi chú |
|---|---|---|
| 0 | raw_rate_rad_s | chưa trừ bias |
| 4 | rate_rad_s | đã lọc, đã trừ bias |
| 8 | angle_rad | tích phân từ lúc boot / RESET_ANGLE |
| 12 | bias_rad_s | |
| 16 | temp_c | |
| 20 | dt_s | |
| 24 | sample_counter | u32 |
| 28 | valid | 1 = đã hiệu chuẩn và mẫu hợp lệ |
| 29 | status | 0 OK, 1 CALIBRATING, 2 NOT_READY, 3 IO_ERROR, 4 BAD_ID, 5 SATURATED, 6 BAD_INPUT |
| 30 | stationary | |
| 31 | simulated | |

ADCS_APP chỉ nên dùng khi `valid == 1 && status == 0`, và kiểm tra tuổi gói như đang làm với LIGHT_DATA.

## Lệnh (CMD MID của imu_app, 0x18D0)
- CC 4 `RECALIBRATE`: hiệu chuẩn lại bias (giữ yên bàn xoay), giữ nguyên góc.
- CC 5 `RESET_ANGLE`: đặt góc về 0.

## Chạy
- Mô phỏng (mặc định): `IMU_APP_BACKEND` không đặt hoặc `sim` (bias 0,21 + nhiễu 0,03 °/s, xung quay 30 °/s trong 4 s mỗi 40 s kể từ giây 25 → +76,4°).
- Phần cứng: `export IMU_APP_BACKEND=i2c` (tuỳ chọn `IMU_APP_I2C_DEVICE`, mặc định `/dev/i2c-1`, địa chỉ 0x68).

## Test trên PC bằng log thật
```
cd apps/imu_app/tests
gcc -O2 -I../fsw/src test_imu_filter.c ../fsw/src/imu_filter.c -lm -o test_imu_filter
./test_imu_filter /đường/dẫn/putty.log
```
