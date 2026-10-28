# ACT_APP – bánh đà phản lực (Nidec 24H055M020), nhận ACT_CMD, publish WHEEL_DATA

Trạng thái: **Mốc 1 — backend mô phỏng.** Logic điều khiển (kẹp, slew, đảo chiều, phanh) đã xong và có test;
driver GPIO/PFM đã viết theo pinout cộng đồng nhưng **chưa chạy trên phần cứng**.

## Luồng

```
SCH_LAB  --WAKEUP_ACT (0x18E3, 10 Hz)--------------------> ACT_APP
ADCS_APP / Ground --ACT_CMD (0x18E0, CC 4 / CC 5)--------> ACT_APP
ACT_APP  --WHEEL_DATA (0x08E4, 10 Hz)--------------------> ADCS_APP / TO_LAB
```

Mỗi WAKEUP: `dt` thật từ `CLOCK_MONOTONIC` → `ACT_MotorStep()` → điền payload → publish → in `>>> ACT`.

## Lệnh (CMD MID 0x18E0)

| CC | Tên | Payload | Ý nghĩa |
|---|---|---|---|
| 4 | `SET_WHEEL_SPEED` (ACT_CMD) | `float omega_rad_s` | Setpoint tốc độ bánh đà, dấu = chiều. ACT tự kẹp về ±`omega_max`. |
| 5 | `STOP_WHEEL` | — | Giảm về 0, phanh, ngắt ENABLE. |

Gửi từ terminal không cần GUI (`cmdUtil` trong `tools/cFS-GroundSystem/Subsystems/cmdUtil`):

```bash
./cmdUtil --host=127.0.0.1 --port=1234 --pktid=0x18E0 --cmdcode=4 --endian=LE --float=120.0
./cmdUtil --host=127.0.0.1 --port=1234 --pktid=0x18E0 --cmdcode=5 --endian=LE
```

## WHEEL_DATA (28 byte payload, offset sau header 12 byte) — `apps/act_app/fsw/inc/act_data.h`

| off | trường | ghi chú |
|---|---|---|
| 0 | cmd_omega_rad_s | setpoint nhận được (đã kẹp) |
| 4 | target_omega_rad_s | sau slew limiter — giá trị đang điều khiển |
| 8 | meas_omega_rad_s | tốc độ: đo từ FG nếu có, hiện là mô hình bậc nhất |
| 12 | pfm_hz | tần số xung đang gửi driver |
| 16 | sample_counter | u32 |
| 20 | valid | 1 = meas_omega dùng được |
| 21 | status | 0 OK, 1 NOT_READY, 2 IO_ERROR, 3 SATURATED, 4 FAULT, 5 BAD_INPUT |
| 22 | saturated | 1 = setpoint bị kẹp |
| 23 | simulated | 1 = backend sim (ADCS không được tin khi chạy động cơ thật) |
| 24 | enabled | chân ENABLE |
| 25 | braking | chân BRAKE đang phanh |
| 26 | direction | +1 CCW, −1 CW, 0 dừng (int8) |
| 27 | measured | 1 = từ FG, 0 = ước lượng open-loop |

ADCS chỉ dùng khi `valid == 1`, kiểm tra tuổi gói như LIGHT_DATA, và đọc `saturated` để chống wind-up.

## Logic động cơ (`act_motor.c`, không phụ thuộc cFS)

1. Kẹp |setpoint| ≤ `omega_max` (mặc định 300 rad/s ≈ 2865 rpm) → `saturated`.
2. Slew limiter `slew_rad_s2` (mặc định 60 rad/s²): động cơ 24H055 bất ổn khi đổi tốc độ đột ngột (forum).
3. **Không đảo DIR khi đang quay**: giảm target về 0, chờ |meas| < `stop_threshold`, mới đổi chiều rồi tăng lại.
4. Tần số PFM = |target| / `rad_s_per_hz`, kẹp trong [`pfm_min`, `pfm_max`] (mặc định 300–10000 Hz; động cơ tự tắt > ~15 kHz).
5. |target| < ngưỡng → PFM 0, BRAKE, ngắt ENABLE.

`rad_s_per_hz = 0.03` là **giá trị giữ chỗ**; phải thay bằng bảng tần số→rpm đo trên bench.

## Backend (`ACT_APP_BACKEND`)

| Giá trị | Hành vi |
|---|---|
| không đặt / `sim` | mô hình bậc nhất τ = 0.8 s, `simulated = 1` |
| `gpio` | Raspberry Pi: PWM phần cứng `/sys/class/pwm/pwmchip0/pwm0` (chu kỳ = 1/f, duty 50 %) + GPIO sysfs ENABLE=23, DIR=24, BRAKE=25 (BCM). Override bằng `ACT_APP_PWM_DIR`, `ACT_APP_GPIO_ENABLE/DIR/BRAKE`. Cần `dtoverlay=pwm` trong `/boot/config.txt`. **Chưa test.** |
| khác | `BAD_INPUT`, không âm thầm mô phỏng |

Pinout 24H055M020 theo cộng đồng (chưa có datasheet chính thức): +12 V, GND, +5 V logic, ENABLE (high = chạy),
BRAKE (low = phanh), DIR (high = CCW), SPEED = PFM (tần số, không phải duty, ~150 Hz–25 kHz). Encoder/FG có thể
không ra chân ngoài → `measured = 0`.

## Test trên PC không cần cFS

```bash
cmake -S tools/act_demo -B build-act-unit
cmake --build build-act-unit -j4
ctest --test-dir build-act-unit --output-on-failure
./build-act-unit/act_demo            # step +150, đảo −100, stop
./build-act-unit/act_demo 80
```

## Checklist trong cFS

- [ ] `Loading file: /cf/act_app.so, APP: ACT_APP` và event `ACT backend=SIMULATION ...`
- [ ] `>>> ACT [1] cmd=0.0 ... en=0 brk=1 sim=1` mỗi 1 s (WAKEUP_ACT đã schedule)
- [ ] `cmdUtil ... --cmdcode=4 --float=120` → event `SET_WHEEL_SPEED 120.00 rad/s`, `>>> ACT` tăng dần đến 120
- [ ] Ground System: trang **WHEEL DATA Tlm** (0x8E4) cập nhật
- [ ] `--float=-50` → `tgt` về 0, `brk=1`, rồi `dir=-1` và tăng lại
- [ ] `--cmdcode=5` → `pfm=0 en=0 brk=1`

## Việc tiếp theo (Mốc 2)

1. Bench test động cơ trần trên Pi: tìm f_min khởi động, dải đảo chiều bất thường, ngưỡng tự tắt, có FG không.
2. Đo tần số → rpm ở ~6 điểm → thay `rad_s_per_hz` (hoặc bảng nội suy như LIGHT).
3. Cân/đo bánh đà in 3D → I_w, chọn `omega_max` an toàn, đưa vào table cFS thay ExampleTable.
4. Nếu có FG: đếm cạnh bằng libgpiod → `measured = 1`.
5. ADCS: subscribe WHEEL_DATA, gửi ACT_CMD từ PID.
