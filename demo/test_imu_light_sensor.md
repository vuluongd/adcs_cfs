# Hướng dẫn test ADCS cFS (`imu_app` / `light_app` / `adcs_app`)

Tài liệu này mô tả cách build, chạy và kiểm tra hệ thống cFS trên branch có `imu_app`, `light_app`, `adcs_app` (repo kiểu `vuluongd/adcs_cfs`, branch `imu_app_thanh` hoặc tương đương).

---

## 1. Mục tiêu test

| Mức | Mục tiêu |
|-----|----------|
| **A. PC / sim** | cFS boot, load app, sim IMU/LIGHT, có log console |
| **B. Ground System** | Laptop nhận telemetry, gửi lệnh |
| **C. Raspberry Pi + cảm biến thật** | I2C IMU (và light), GS trên laptop qua LAN |

---

## 2. Yêu cầu môi trường

### 2.1. Máy dev (Ubuntu / Debian)

```bash
sudo apt update
sudo apt install -y git build-essential cmake gcc g++ python3 python3-pyqt5 python3-zmq
```

### 2.2. Clone & submodule

```bash
git clone -b imu_app_thanh https://github.com/vuluongd/adcs_cfs.git
cd adcs_cfs
git submodule update --init --recursive
```

---

## 3. Chuẩn bị build (các lỗi đã gặp)

### 3.1. Cờ `-std=c99` làm fail C++

**Triệu chứng:** `command-line option '-std=c99' is valid for C/ObjC but not for C++`

**Sửa** `sample_defs/arch_build_custom.cmake`:

```cmake
add_compile_options(
    $<$<COMPILE_LANGUAGE:C>:-std=c99>
    -pedantic
    -Wall
    -Werror
)
```

### 3.2. Unit test thiếu stub

**Triệu chứng:** `undefined reference to IMU_APP_RecalibrateCmd` / `IMU_APP_ResetAngleCmd`

**Cách nhanh — tắt unit test** trong `target-configs.mk`:

```make
PREP_OPTS_native_std += -DENABLE_UNIT_TESTS=FALSE
```

(Lưu ý: `native_std` **hardcode** `ENABLE_UNIT_TESTS=TRUE`; chỉ truyền biến môi trường khi `prep` thường **không** đủ.)

**Cách đúng:** thêm stub vào `apps/imu_app/unit-test/stubs/imu_app_cmds_stubs.c` cho hai hàm trên.

### 3.3. Schedule wakeup cho IMU

Không có dòng schedule thì app init nhưng **không** có `>>> IMU`.

Sửa `sample_defs/tables/sch_lab_table.c`:

- Include MID (có thể cần copy `imu_app_topicids.h` vào `build-.../inc` nếu thiếu path).
- Thêm entry, ví dụ 10 Hz (`TickRate = 10`):

```c
{CFE_SB_MSGID_WRAP_VALUE(IMU_APP_WAKEUP_MID), 1, 0}, /* IMU 10 Hz */
{CFE_SB_MSGID_WRAP_VALUE(LIGHT_APP_WAKEUP_MID), 2, 0}, /* LIGHT 5 Hz */
```

Nếu include fail, dùng MID số (thường TLM/CMD theo topic; LIGHT DATA đã dùng `0x08DC` trong TO).

### 3.4. TO_LAB forward telemetry

`sample_defs/tables/to_lab_sub.c` — ví dụ:

```c
{CFE_SB_MSGID_WRAP_VALUE(0x08DC), {0, 0}, 4}, /* LIGHT_APP DATA */
{CFE_SB_MSGID_WRAP_VALUE(0x08D4), {0, 0}, 4}, /* IMU_APP DATA — xác nhận MID trên build của bạn */
```

### 3.5. Ground System — trang telemetry

1. Tạo `tools/cFS-GroundSystem/Subsystems/tlmGUI/imu-app-data-tlm.txt` (offset sau header 12 byte, khớp `imu_data.h`).
2. Thêm vào `telemetry-pages.txt`:

```text
LIGHT DATA Tlm,  GenericTelemetry.py,  0x8DC,  light-app-data-tlm.txt
IMU DATA Tlm,    GenericTelemetry.py,  0x8D4,  imu-app-data-tlm.txt
```

---

## 4. Build & chạy trên PC (mô phỏng)

```bash
cd ~/adcs_cfs
rm -rf build-native_std    # khi đổi ENABLE_UNIT_TESTS hoặc cmake options

make native_std.prep
make native_std.install

cd build-native_std/exe/cpu1
unset IMU_APP_BACKEND      # hoặc: export IMU_APP_BACKEND=sim
./core-cpu1
```

### 4.1. Checklist log boot

- [ ] `Opened ES App Startup file: /cf/cfe_es_startup.scr`
- [ ] `Loading file: /cf/imu_app.so, APP: IMU_APP`
- [ ] `Loading file: /cf/light_app.so`
- [ ] `Loading file: /cf/adcs_app.so`
- [ ] `IMU_APP Initialized`
- [ ] `IMU backend=SIMULATION ...`
- [ ] `CFE_ES_Main entering OPERATIONAL state`
- [ ] `>>> LIGHT [...] sim=1` (nếu light đã schedule)
- [ ] `>>> IMU [...] sim=1` (nếu IMU đã schedule)
- [ ] Event ADCS nhận light: `LIGHT_DATA n=...` (nếu adcs subscribe light)

### 4.2. Lỗi chấp nhận được khi test local

| Log | Ý nghĩa |
|-----|---------|
| `setschedparam ... not permitted` | Không có quyền RT priority |
| `Could not load ... sbn.so` | SBN thiếu dependency |
| `Msg Limit Err` / `Pipe Overflow` TO_LAB | Chưa bật Ground System nhận tlm |

---

## 5. Test Ground System (laptop, cFS local)

**Terminal 1 — cFS:**

```bash
cd ~/adcs_cfs/build-native_std/exe/cpu1
./core-cpu1
```

**Terminal 2 — Ground System:**

```bash
cd ~/adcs_cfs/tools/cFS-GroundSystem
# lần đầu có thể cần:
# cd Subsystems/cmdUtil && make && cd ../..
python3 GroundSystem.py
```

**Trên GUI:**

1. Start **Command System**
2. **Telemetry Output (TO)** → Enable → IP `127.0.0.1`
3. Start **Telemetry System**
4. Mở trang **IMU DATA Tlm** / **LIGHT DATA Tlm**

**Kỳ vọng:** giá trị cập nhật; `simulated = 1` khi backend sim.

**Lệnh thử (nếu đã thêm command page):** NOOP, RECALIBRATE, RESET_ANGLE cho IMU.

---

## 6. Test trên Raspberry Pi + cảm biến thật

### 6.1. Phần cứng

- MPU6050: I2C (`SDA`/`SCL`), 3.3V, GND, địa chỉ thường `0x68`
- Cảm biến light: theo thiết kế `light_app`

```bash
sudo raspi-config   # bật I2C
sudo apt install -y i2c-tools
i2cdetect -y 1      # thấy 0x68
sudo usermod -aG i2c $USER   # rồi login lại
```

### 6.2. Build trên Pi

```bash
# Cùng các sửa cmake / schedule / TO như trên PC
make native_std.prep    # với UT tắt nếu cần
make native_std.install
```

### 6.3. Chạy với I2C thật

```bash
cd ~/adcs_cfs/build-native_std/exe/cpu1
export ACT_APP_BACKEND=gpio
export ACT_APP_PWM_DIR=/sys/class/pwm/pwmchip0/pwm0
export ACT_APP_GPIO_ENABLE=23
export ACT_APP_GPIO_DIR=24
export ACT_APP_GPIO_BRAKE=25
export IMU_APP_BACKEND=i2c
export IMU_APP_I2C_DEVICE=/dev/i2c-1
# LIGHT: theo env của light_app nếu có (ví dụ LIGHT_APP_BACKEND=...)
./core-cpu1

```

**Kỳ vọng:**

- [ ] `IMU backend=I2C` (không phải SIMULATION)
- [ ] `>>> IMU [...] sim=0`
- [ ] Xoay board → `rate` / `angle` đổi
- [ ] Light: lux / bearing đổi khi chiếu sáng

### 6.4. Ground System trên laptop (cFS trên Pi)

1. Pi và laptop cùng LAN; ghi IP Pi và IP laptop (`hostname -I`).
2. Pi: chạy `./core-cpu1` như trên.
3. Laptop: `python3 GroundSystem.py`
4. TO Enable → **IP của laptop** (nơi nhận telemetry).
5. Command → gửi tới **IP Pi**, port **1234** (CI_LAB).
6. Xem trang IMU / LIGHT.

---

## 7. Ma trận test nhanh

| # | Kịch bản | Cách làm | Pass khi |
|---|----------|----------|----------|
| T1 | Boot cFS | `./core-cpu1` | OPERATIONAL, load đủ app |
| T2 | IMU sim | `unset IMU_APP_BACKEND` | `backend=SIMULATION`, có `>>> IMU` |
| T3 | LIGHT sim | (mặc định light) | có `>>> LIGHT` |
| T4 | ADCS nhận light | quan sát EVS | event `LIGHT_DATA n=` |
| T5 | GS local | TO enable 127.0.0.1 | trang tlm cập nhật |
| T6 | IMU I2C | `IMU_APP_BACKEND=i2c` trên Pi | `sim=0`, angle đổi khi xoay |
| T7 | GS remote | Pi + laptop LAN | tlm trên GS, lệnh NOOP có event |
| T8 | RECALIBRATE | lệnh ground / CC | event recalibrate, platform đứng yên |
| T9 | RESET_ANGLE | lệnh ground | angle về ~0 |

---

## 8. `adcs_app` — hướng test sau khi có sensor apps

Hiện ADCS chủ yếu **subscribe `LIGHT_DATA`**. Sau khi bổ sung IMU:

| Bước test | Nội dung |
|-----------|----------|
| Subscribe | Init có `CFE_SB_Subscribe(IMU_APP_DATA_MID, ...)` |
| Receive | Packet đúng size → copy payload, không reject |
| Usable | `valid`, `status`, tuổi dữ liệu (timestamp) |
| Fusion (khi đã code) | Góc / rate hợp lý so với light + imu |
| Actuator (khi nối `act_app`) | Lệnh SB tới act khi có sai số |

Checklist code gợi ý (chưa bắt buộc để pass T1–T7):

- [ ] `ADCS_APP_ReceiveImu` tương tự `ADCS_APP_ReceiveLight`
- [ ] `ADCS_APP_ImuUsable(allow_simulated)`
- [ ] (Tuỳ chọn) wakeup ADCS trong `sch_lab` + publish telemetry ADCS

---

## 9. Biến môi trường hữu ích

| Biến | Ý nghĩa |
|------|---------|
| `IMU_APP_BACKEND=sim` hoặc không set / khác `i2c` | Mô phỏng (không cần cảm biến) |
| `IMU_APP_BACKEND=i2c` | MPU6050 qua i2c-dev |
| `IMU_APP_I2C_DEVICE` | Mặc định `/dev/i2c-1` |

---

## 10. Dừng cFS

Trong terminal đang chạy `./core-cpu1`: **Ctrl+C**.

Có thể thấy log exception SIGINT / POWERON Reset — bình thường khi shutdown tay.

---

## 11. Tài liệu / đường dẫn chính

| Thành phần | Đường dẫn |
|------------|-----------|
| Schedule | `sample_defs/tables/sch_lab_table.c` |
| TO subscribe | `sample_defs/tables/to_lab_sub.c` |
| IMU source | `apps/imu_app/fsw/src/` |
| IMU data ICD | `apps/imu_app/fsw/inc/imu_data.h` |
| ADCS + light | `apps/adcs_app/fsw/src/adcs_app_light.c` |
| Ground System | `tools/cFS-GroundSystem/` |
| Tlm pages | `tools/cFS-GroundSystem/Subsystems/tlmGUI/telemetry-pages.txt` |
| Build config | `target-configs.mk`, `sample_defs/arch_build_custom.cmake` |

---

## 12. Quy trình đề xuất một buổi test

1. PC: build + sim → T1, T2, T3, T4  
2. PC: Ground System localhost → T5  
3. Pi: I2C detect + cFS `i2c` → T6  
4. Laptop GS + Pi → T7  
5. Lệnh RECALIBRATE / RESET_ANGLE → T8, T9  
6. (Sau khi code xong) fusion / act → mục 8  

---

*Tài liệu tổng hợp từ quá trình tích hợp và debug cFS ADCS (imu / light / ground / Pi). Điều chỉnh MID hex theo đúng build của mission nếu khác `0x08D4` / `0x08DC`.*
