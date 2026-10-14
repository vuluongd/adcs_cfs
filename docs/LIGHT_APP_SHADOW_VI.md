# LIGHT_APP V2 — hai BH1750 song song có vách chắn, C/cFS

Đây là bản phù hợp sơ đồ hai cảm biến 1/2 nằm hai bên một tấm chắn. Bản trước
dành cho cảm biến nghiêng ±45°; công thức atan của bản đó không áp dụng ở đây.

## Dữ liệu gửi cho ADCS ngay bây giờ

Sau trừ offset và nhân gain mỗi kênh, LIGHT tính:

```c
float error = (lux_2 - lux_1) / (lux_2 + lux_1);
```

Đây là đại lượng không có đơn vị, trong [-1,+1], KHÔNG phải radian hay độ.
Error dương: cảm biến phải (2) sáng hơn. Error âm: cảm biến trái (1) sáng hơn.
Error gần 0 chỉ gợi ý cân bằng sáng; chưa chứng minh hướng đúng vì bóng có thể
chưa chạm chip, hai chip cùng bị che, hoặc ánh sáng nền chiếm ưu thế.

Ví dụ 400/600 lux → error=+0.2; 600/400 lux → error=-0.2; 500/500 → error=0.
Không gán các số này thành +20°/-20°. Khi tổng sáng <20 lux, lỗi I2C, bão hòa
hoặc số không hợp lệ: error_valid=0. Ngưỡng 20 lux là tham số khởi đầu cần chỉnh.

Payload LIGHT_DATA giữ các trường left_lux/right_lux, normalized_diff,
bearing_rad, sample_counter, valid, status, simulated; byte reserved của bản
V1 nay mang error_valid. Cần build lại cả LIGHT_APP và ADCS_APP, không chạy
lẫn binary V1/V2. Bộ lọc EMA có alpha=0.5, wakeup mỗi 200ms như trước.

| Trường | Khi chưa hiệu chuẩn góc |
|---|---|
| normalized_diff | Error đã lọc, có thể dùng nếu error_valid=1 |
| error_valid | 1 nếu phép tính error hợp lệ |
| bearing_rad | 0 là giá trị giữ chỗ, KHÔNG được dùng như góc |
| valid | 0: chưa có góc hợp lệ |
| status | 7 = NO_CALIBRATION nếu số lux tốt nhưng chưa có bảng góc |
| simulated | 1 cho dữ liệu giả, 0 cho I2C thật |

## Chạy C độc lập

Giải nén ZIP và mở terminal Linux/WSL/VM trong thư mục standalone:

```bash
cmake -S . -B build
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/light_demo 400 600
```

Kết quả: error=+0.2000, error_valid=1, angle_valid=0, status=7, angle=N/A.
Chạy không tham số để thử nhiều số lux giả. Những số này không mô phỏng hình
học vách chắn và không có góc thật. Bản standalone có hai test; trong repo có
thêm test phần ADCS receiver, tổng cộng ba test.

Đọc phần cứng BH1750 trên Linux:

```bash
LIGHT_APP_I2C_DEVICE=/dev/i2c-1 ./build/light_demo --i2c
```

Driver dùng địa chỉ 7-bit 0x23 (LEFT/1) và 0x5C (RIGHT/2), continuous
H-resolution mode1, MTreg=69. Hai cảm biến dùng chung SDA/SCL/GND, chọn địa
chỉ khác nhau bằng ADDR/jumper; nếu cả hai module cố định cùng địa chỉ, cần
bus riêng hoặc multiplexer. Logic I2C phải phù hợp 3.3V của board. VM chỉ đọc
phần cứng khi có adapter/bus thật được hệ điều hành cấp.

Phần tính error là C độc lập, có thể dùng trên ESP32. Phần light_sensor.c trong
gói là driver Linux/Raspberry Pi; không nạp trực tiếp driver Linux vào ESP32.

## Patch cFS

Gói có hai patch. Chạy ở thư mục gốc repo vuluongd/adcs_cfs:

- Nếu CHƯA áp dụng bản V1: dùng light_app_shadow_from_base.patch, commit nền
  91ae348df26ef91388310a43a26edcc18d3f7702.
- Nếu ĐÃ áp dụng đúng patch V1 từ gói trước: dùng light_app_shadow_upgrade_v1.patch.

Kiểm tra trước khi áp dụng, không dùng cả hai patch:

```bash
git apply --check /duong/dan/light_app_shadow_from_base.patch
git apply /duong/dan/light_app_shadow_from_base.patch
```

Nếu có local edits hoặc khác phiên bản khiến --check thất bại, đối chiếu thay
đổi thay vì ghi đè source đang có. Patch chưa được commit/push lên remote.

Sau khi áp dụng, build/test code độc lập trong repo:

```bash
cmake -S tools/light_demo -B build-light-unit
cmake --build build-light-unit -j4
ctest --test-dir build-light-unit --output-on-failure
./build-light-unit/light_demo 400 600
```

Profile cFS/Linux non-EDS vẫn sử dụng light_demo_defs, có SCH_LAB gọi LIGHT và
ADCS đăng ký LIGHT_DATA. Không có PWM hay điều khiển bánh xe trong profile này.

```bash
git submodule update --init --depth 1 cfe osal psp libs/sample_lib apps/sch_lab tools/elf2cfetbl tools/tblCRCTool tools/commandline-tools
cmake -S cfe -B build-light-cfs -DMISSIONCONFIG=light_demo -DSIMULATION=native -DCFE_EDS_ENABLED=OFF -DENABLE_UNIT_TESTS=OFF -DCMAKE_INSTALL_PREFIX="$PWD/run-light"
cmake --build build-light-cfs --target mission-install -j4
cd run-light/cpu1
LIGHT_APP_BACKEND=sim ./core-cpu1
```

Chạy bằng tài khoản Linux thường. Trên máy gắn I2C thật, thay dòng cuối bằng:

```bash
LIGHT_APP_BACKEND=i2c LIGHT_APP_I2C_DEVICE=/dev/i2c-1 ./core-cpu1
```

ADCS dùng ADCS_APP_LightErrorUsable(false) để kiểm tra error trên phần cứng;
dùng true chỉ khi thử giả lập. ADCS_APP_LightUsable(false) kiểm tra GÓC đã
hiệu chuẩn, sẽ trả false khi chưa có bảng góc. Cả hai kiểm tra tuổi gói ≤600ms
và cần được gọi mỗi chu kỳ điều khiển. Đây là phần nhận dữ liệu, chưa là PID.

## Bổ sung góc sau khi đo thật

Đầu tiên cân bằng gain/offset tại góc 0°; đặt filter_alpha=1 khi ghi mẫu tĩnh.
Giữ đèn/khoảng cách, đo ở các góc đã biết của mô hình và tính error từ lux.
Chọn vùng mà error thay đổi đơn điệu theo góc; không dùng vùng bóng chưa chạm
chip hoặc chip đã bị che kín khiến nhiều góc cho gần cùng error. Nếu không
có vùng đủ nhạy quanh 0°, cần chỉnh cơ khí trước, không sửa bằng một hệ số.

LIGHT_CalPoint_t có hai cột normalized_diff và bearing_rad. Khai báo mảng
static const ở phạm vi file từ dữ liệu thực của bạn (không dùng mảng local
trên stack rồi giữ con trỏ sau khi hàm kết thúc), rồi sau LIGHT_DefaultConfig(&Config) trong
light_app_tracking.c, gán:

```c
Config.calibration = YourMeasuredTable;
Config.calibration_count = sizeof(YourMeasuredTable) / sizeof(YourMeasuredTable[0]);
```

Mảng 2..32 điểm, error tăng nghiêm ngặt và góc radian tăng theo hướng RIGHT.
Code nội suy tuyến tính giữa các điểm, từ chối bảng trùng/không hữu hạn/không
đơn điệu, không ngoại suy ngoài vùng đo. Ngoài bảng: error vẫn có thể hợp lệ
nhưng angle_valid=0. Sau đổi gain/offset/bảng cần reset bộ lọc. Không có bảng
góc mặc định giả trong code; bảng trong test chỉ để kiểm tra nội suy.

Tính mômen/PID bằng error không thứ nguyên và tính bằng radian có gain khác
nhau. Không đổi đại lượng đầu vào mà giữ nguyên gain PID được chỉnh cho độ.
Việc kiểm tra chiều mômen bánh xe, gyro và giới hạn actuator thuộc ADCS/ACT.

Kết quả build/test nằm trong VALIDATION.md. Chưa đo góc trên BH1750 thật.
