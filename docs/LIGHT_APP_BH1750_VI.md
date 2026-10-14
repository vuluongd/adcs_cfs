# LIGHT_APP bằng C — hai BH1750, ADCS một trục

> Tài liệu lưu của bản V1, bố trí cảm biến nghiêng ±45°. Code hiện tại đã chuyển
> sang bố trí song song có vách chắn. Dùng LIGHT_APP_SHADOW_VI.md cho bản mới.

Bản đầu dành cho mô hình học tập quay chậm. Được viết dựa trên repository
`https://github.com/vuluongd/adcs_cfs`, commit
`91ae348df26ef91388310a43a26edcc18d3f7702`.

## 1. Ý tưởng bằng lời

Bạn muốn mặt trước mô hình hướng về một đèn. Gắn hai BH1750 lên phần thân cùng
quay với mô hình. Mặt nhận sáng của cảm biến LEFT nghiêng về trái 45°, mặt
nhận sáng RIGHT nghiêng về phải 45°, tính từ hướng trước của mô hình. Đèn
nằm ở phía trước và tương đối xa so với khoảng cách giữa hai cảm biến.

Khi mô hình hướng đúng về đèn, hai cảm biến đã hiệu chuẩn đọc gần bằng nhau.
Đèn lệch phải thì RIGHT sáng hơn, lệch trái thì LEFT sáng hơn. LIGHT_APP biến
hai số lux thành góc lệch của đèn so với thân. ADCS_APP nhận góc này và dùng
nó cùng dữ liệu gyro để tính điều khiển bánh xe động lượng.

Không đặt hai cảm biến song song, cùng hướng rồi mong đo góc từ chênh lệch:
với ánh sáng xa, chúng có thể đọc gần bằng nhau dù mô hình đang lệch hướng.
Phương án trong code dùng hai mặt nhận sáng nghiêng đối xứng, không dùng vách
che bóng. Nếu dùng vách che, cần một mô hình/đường hiệu chuẩn khác.

BH1750 đo độ rọi; góc trong code là **ước lượng theo mô hình cosine lý tưởng**,
không phải đại lượng cảm biến trả trực tiếp, và chưa được xác nhận bằng phần
cứng. Hai đồ thị đáp ứng góc trong datasheet không đảm bảo cảm biến/module
thực tế tuân đúng mô hình này. Cần hiệu chuẩn bằng các góc đã biết.

## 2. Phân công đúng theo bảng APP bạn gửi

| Thành phần | Nhận | Thực hiện | Gửi |
|---|---|---|---|
| SCH_LAB | Lịch | Phát wakeup mỗi 200 ms | WAKEUP_LIGHT |
| LIGHT_APP | WAKEUP_LIGHT | Đọc LEFT/RIGHT, hiệu chỉnh, kiểm tra lỗi, lọc và ước lượng góc | LIGHT_DATA |
| ADCS_APP | LIGHT_DATA; về sau thêm IMU_DATA | Lưu góc nguồn sáng, kiểm tra dữ liệu mới; phần PID cần bổ sung | Về sau ACT_CMD |
| ACT_APP | Về sau ACT_CMD | Điều khiển ESC/bánh xe | Về sau WHEEL_DATA |

Bản này thực hiện LIGHT_APP và **phần nhận LIGHT_DATA của ADCS_APP**. Các app
ADCS/IMU/ACT trong repo hiện là khung mẫu. Bản này chưa thực hiện PID hai vòng,
đọc gyro, PWM/ESC hoặc điều khiển động cơ. Nhận được LIGHT_DATA chưa có nghĩa
là mô hình đã có vòng điều khiển kín.

`SUB` là đăng ký nhận một loại message qua Software Bus. `PUB` là gửi message.
LIGHT và ADCS không gọi trực tiếp hàm của nhau, không dùng một biến toàn cục
chung. Hai app dùng chung định nghĩa kiểu gói tin trong `light_data.h`.

## 3. Góc tương đối và góc mục tiêu

Đặt `theta` là góc của đèn so với hướng trước của thân, dương về phía RIGHT,
và `alpha=45°` là góc nghiêng mỗi cảm biến. Với hai cảm biến cosine đã cân bằng:

```
L = A*cos(theta + alpha)
R = A*cos(theta - alpha)
d = (R-L)/(R+L)
theta = atan(d/tan(alpha))
```

Khi alpha=45°, `theta=atan(d)`. Trong C:

```c
float d = (right_lux - left_lux) / (right_lux + left_lux);
float bearing_rad = atanf(d);  /* radian, sau khi kiểm tra điều kiện đo */
```

Ví dụ không lọc: L≈422.62 lux, R≈906.31 lux → góc lý tưởng +20°.
L=R=707.11 lux → 0°. Hai số bằng nhau chỉ gợi ý hướng đúng khi đèn chính
chiếm ưu thế; ánh sáng tán xạ đều hoặc nhiều đèn cũng có thể cho L≈R.

**bearing_rad không phải góc thái độ tuyệt đối của thân.** Có hai cách ADCS dùng:

1. Bám sáng trực tiếp: sai số điều khiển `e = bearing_rad`, mục tiêu e→0.
2. Nếu bộ PID có góc thân trong một hệ quy chiếu xác định:
   `theta_target = wrap(theta_body_estimated + bearing_rad)`. Lấy ước lượng
   góc thân cùng thời điểm phép đo; tất cả phải cùng chiều dương và đơn vị.

Không đặt `theta_target = bearing_rad` rồi trừ tiếp góc IMU: đó là trộn góc
tương đối và góc tuyệt đối. Với bánh xe động lượng, mômen tác dụng lên thân
ngược dấu mômen tăng tốc bánh xe; ACT/ADCS phải kiểm tra quy ước dấu thực tế.

## 4. Nối hai BH1750 trên cùng I²C

| Chân module | LEFT | RIGHT |
|---|---|---|
| VCC/VIN | Nguồn phù hợp module, ưu tiên logic 3.3 V | Chung nguồn |
| GND | GND chung | GND chung |
| SDA | SDA của máy đọc | Cùng SDA |
| SCL | SCL của máy đọc | Cùng SCL |
| ADDR | Mức thấp → 0x23 | Mức cao → 0x5C |

Địa chỉ 0x23/0x5C là địa chỉ 7-bit, không dịch trái khi truyền cho I2C_SLAVE.
Với Raspberry Pi, bus thường là `/dev/i2c-1`. Cần bật I²C và cấp quyền truy cập
bus cho tài khoản chạy cFS. Dùng mức SDA/SCL 3.3 V phù hợp board. Không áp
dụng khả năng chịu 5 V của một module có regulator/level shifter cho mọi
module BH1750: bản thân IC có dải VCC 2.4–3.6 V theo datasheet.

Nếu module không đưa chân ADDR ra, kiểm tra jumper đổi địa chỉ. Nếu cả hai
module cố định 0x23 thì cần bus khác hoặc I²C multiplexer; không đọc độc lập
được hai con cùng địa chỉ trên một bus. Không tự đổi sang địa chỉ tùy ý.

## 5. Gói LIGHT_DATA và điều kiện sử dụng

Payload C có các trường:

| Trường | Nghĩa |
|---|---|
| left_lux/right_lux | Số đo hiện tại sau trừ offset và nhân gain |
| normalized_diff | (R-L)/(R+L) sau lọc EMA |
| bearing_rad | Góc tương đối, radian; chỉ dùng khi valid=1 |
| sample_counter | Số thứ tự wakeup, kể cả lần đo lỗi |
| valid | 1 khi phép đo qua các kiểm tra số học/biên |
| status | OK/DARK/SATURATED/OUT_OF_VIEW/IO_ERROR/NOT_READY/BAD_INPUT |
| simulated | 1 cho dữ liệu giả, 0 cho I²C thật |

Header cFE chứa thời gian phát. `ADCS_APP_LightUsable(allow_simulated)` kiểm tra
gói hợp lệ, giá trị hữu hạn, biên góc và tuổi gói ≤600 ms. Gọi hàm này ở **mỗi
chu kỳ điều khiển**: nếu LIGHT bị dừng mà ADCS vẫn giữ cache cũ, phải nhận ra
dữ liệu đã hết hạn. Trên phần cứng dùng `allow_simulated=false`. Không coi góc
0 trong một gói invalid là đã hướng đúng đèn.

`valid=1` không chứng minh môi trường có một đèn duy nhất, và không chứng minh
góc đã được hiệu chuẩn chính xác. Hai cảm biến không giải được mọi tình huống
360°, nhiều nguồn sáng hoặc ánh sáng tán xạ. Bản đầu giới hạn vùng làm việc
lý tưởng ±35° phía trước và tránh để mô hình quay ra phía sau đèn.

Các tham số đầu: tổng sáng tối thiểu 20 lux, mỗi kênh ≥4% tổng, góc tối đa
35°, EMA alpha=0.5, wakeup 5 Hz. Đây là **tham số khởi đầu**, phải chỉnh theo
mô hình thật. Khi invalid, bộ lọc được reset để mẫu cũ không che một lỗi mới.

Driver dùng continuous H-resolution mode1, MTreg=69, raw/1.2. Sau khởi tạo,
đợi ít nhất 180 ms trước đọc lần đầu; handler không sleep để đợi chuyển đổi.
Hai cảm biến được đọc nối tiếp rất gần nhau, chưa có đồng bộ phần cứng.
BH1750 không có ID chu kỳ đo để chứng minh mẫu mới; đây là thiết kế cho mô
hình quay chậm. Độ trễ chuyển đổi và EMA phải được tính khi thiết kế PID.

## 6. Thử C trước, chưa cần chạy toàn bộ cFS

Trong repo đã áp dụng patch, dùng Linux/WSL/VM với GCC và CMake:

```bash
cmake -S tools/light_demo -B build-light-unit
cmake --build build-light-unit -j4
ctest --test-dir build-light-unit --output-on-failure
./build-light-unit/light_demo
./build-light-unit/light_demo 422.62 906.31
```

Lệnh cuối in góc khoảng +20°. Với số lux=0, chương trình in invalid và trả
exit code 1. Bản standalone trong ZIP chạy cùng thuật toán mà không cần repo:

```bash
cd standalone
cmake -S . -B build
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/light_demo
```

## 7. Áp dụng patch vào repository của bạn

Giải nén ZIP. Đặt file patch cạnh repo hoặc sửa đường dẫn dưới đây. Chạy ở thư
mục gốc repo. Kiểm tra trước; nếu phiên bản/local edits khác và `--check` thất
bại, không ghi đè app đang có, hãy đối chiếu thay đổi.

```bash
git apply --check /duong/dan/light_app_bh1750.patch
git apply /duong/dan/light_app_bh1750.patch
```

Commit nền của patch ghi ở đầu tài liệu. Patch chưa được commit/push lên remote.

## 8. Chạy cFS với dữ liệu giả trên Linux/VM

Profile `light_demo_defs` chỉ chạy SAMPLE_LIB, LIGHT_APP, ADCS_APP và SCH_LAB.
Nó không khởi động ACT, không phát PWM, không điều khiển bánh xe.
Không dùng lại build directory của một mission khác.

```bash
git submodule update --init --depth 1 cfe osal psp libs/sample_lib apps/sch_lab tools/elf2cfetbl tools/tblCRCTool tools/commandline-tools
cmake -S cfe -B build-light-cfs -DMISSIONCONFIG=light_demo -DSIMULATION=native -DCFE_EDS_ENABLED=OFF -DENABLE_UNIT_TESTS=OFF -DCMAKE_INSTALL_PREFIX="$PWD/run-light"
cmake --build build-light-cfs --target mission-install -j4
cd run-light/cpu1
LIGHT_APP_BACKEND=sim ./core-cpu1
```

Chạy bằng tài khoản Linux thông thường. Profile native cho phép OSAL dùng chế
độ permissive đối với ưu tiên luồng và giới hạn hàng đợi của môi trường mô
phỏng. Bản này hỗ trợ **Linux, cFE non-EDS**; EDS chưa được bổ sung và build
sẽ báo rõ nếu bật EDS. `SIMULATION=native` nghĩa là build cho máy hiện tại;
nó không tự buộc dữ liệu cảm biến thành giả. Backend do biến môi trường chọn.

ADCS in message thứ 1 và mỗi 10 mẫu, khoảng mỗi 2 giây khi chạy bình thường:

```
LIGHT_DATA sample=10 bearing=...deg valid=1 status=0 simulated=1
```

## 9. Chuyển sang cảm biến thật

Trên máy Linux gắn I²C thật, thử driver độc lập trước:

```bash
./build-light-unit/light_demo --i2c
```

Sau đó chạy cùng profile cFS với:

```bash
cd run-light/cpu1
LIGHT_APP_BACKEND=i2c LIGHT_APP_I2C_DEVICE=/dev/i2c-1 ./core-cpu1
```

Mẫu đầu có thể NOT_READY (5). Rút một cảm biến: phải thành IO_ERROR (4), valid=0.
Sau khi nối lại, driver thử khởi tạo lại, đợi chuyển đổi rồi phục hồi. Chế độ
mặc định là `sim`; trên phần cứng luôn đặt `LIGHT_APP_BACKEND=i2c` rõ ràng.
VM không tự có bus I²C của Raspberry Pi; muốn phần cứng phải có bus/adapter
thực sự được hệ điều hành cấp thành thiết bị I²C.

## 10. Hiệu chuẩn trước khi làm PID

1. Đặt nguồn sáng trước mô hình tại góc 0°, giữ nguồn ở khoảng cách cố định.
2. Ghi hai số khi tắt nguồn sáng chính để ước lượng nền/offset. Nền có thể
   phụ thuộc góc và không luôn trừ được bằng một hằng số.
3. Với các số đã trừ offset ở 0°, chọn left_gain=1,
   right_gain=L0/R0 (khi R0>0). Sửa LIGHT_DefaultConfig rồi build lại.
4. Đo ở -30°, -20°, -10°, 0°, +10°, +20°, +30°. Tạm đặt filter_alpha=1
   để so sánh số đo tĩnh, không bị trễ EMA.
5. So góc ước lượng với góc đã biết. Nếu sai số lớn hoặc đáp ứng không phù
   hợp cosine, thay công thức bằng lookup table từ dữ liệu hiệu chuẩn.
6. Xác nhận chiều dương bằng cách đưa đèn sang RIGHT. Góc phải dương. Sau đó
   mới kết hợp gyro, PID, giới hạn tốc độ bánh xe và kiểm tra dấu mômen.

Các file chính: `light_tracking.c` (ước lượng), `light_sensor.c` (BH1750/Linux
và giả lập), `light_app_tracking.c` (publish cFS), `adcs_app_light.c` (receive
và freshness), `light_data.h` (định dạng gói).

Nguồn kỹ thuật:
- ROHM BH1750FVI datasheet: https://www.mouser.com/datasheet/2/348/bh1750fvi-e-186247.pdf
- NASA cFS sample app: https://github.com/nasa/sample_app
- Adafruit module pinouts: https://learn.adafruit.com/adafruit-bh1750-ambient-light-sensor/pinouts

Kết quả kiểm tra thực tế của gói nằm trong `VALIDATION.md`.
