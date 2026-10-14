# Ba file C: góc, dữ liệu ảo, chân I2C

Phiên bản này bổ sung đúng ba file .c vào apps/light_app/fsw/src, có khai báo
hàm trong hai header sẵn có light_tracking.h và light_sensor.h. CMake và các
file gọi hàm đã được cập nhật để sử dụng các module mới, không chỉ chép ba file
rời rồi bỏ đó. Driver thật hiện dành cho Linux/Raspberry Pi.

## 1. light_angle.c — xác định góc

Hàm LIGHT_AngleFromError(&config,error,&angle_rad) nhận chênh lệch sáng chuẩn
hóa và bảng hiệu chuẩn, kiểm tra bảng rồi nội suy góc. Dương về cảm biến 2/right,
âm về 1/left. Mã trả về:

- 0/OK: angle_rad hợp lệ.
- 7/NO_CALIBRATION: chưa có bảng thật; không dùng giá trị góc giữ chỗ bằng 0.
- 3/OUT_OF_VIEW: error nằm ngoài vùng đã hiệu chuẩn, không ngoại suy.
- 6/BAD_INPUT: bảng/số đo/đối số không hợp lệ.

Trong LIGHT_APP, light_tracking.c gọi module này sau kiểm tra lux và lọc EMA.
Bảng hiệu chuẩn thật phải được đo trên đúng hai BH1750 + vách chắn của bạn;
không dùng bảng atan hay bảng ảo để khẳng định góc vật lý. Các cặp error/góc
phải tăng nghiêm ngặt trong vùng một-một; vùng bóng phẳng không đảo ra góc được.

## 2. light_sim.c — tạo dữ liệu ảo

LIGHT_SimGenerate(step,mode,&lux1,&lux2,&true_angle_rad) tạo lux và góc tham
chiếu. Bốn chế độ: LIGHT_SIM_SWEEP, LIGHT_SIM_DARK, LIGHT_SIM_IO_ERROR,
LIGHT_SIM_SATURATED. Góc sweep +/-30°, chu kỳ 80 mẫu (16 giây nếu 5 Hz).

Mô hình ảo đặt error=0.6*sin(phase), góc=30°*sin(phase), tổng sáng 1400 lux.
Đây là quan hệ giả có chủ đích để kiểm tra phần mềm, KHÔNG là mô hình vật lý
đã được xác nhận cho BH1750. LIGHT_UseSimulationCalibration(&config) gắn bảng
phù hợp mô hình này. App chỉ gọi hàm gắn bảng ảo khi backend=sim, luôn gửi
simulated=1. Khi backend=i2c, app không dùng bảng ảo; góc vẫn invalid cho đến
khi thêm dữ liệu hiệu chuẩn thật. ADCS không được cho phép simulated khi chạy
động cơ thật.

Chạy demo với filter_alpha=1 để thấy góc tính khớp góc tham chiếu. Trong app,
alpha=0.5 làm góc đã lọc trễ so với góc ảo hiện tại, đó là tác dụng của EMA.

## 3. light_pins.c — xác định chân theo board

LIGHT_GetPinConfig(board) trả cấu hình GPIO, chân vật lý, địa chỉ cảm biến
và đường dẫn bus. Bạn chủ động chọn board; module không tự dò dây hoặc tự
đổi pin mux trên phần cứng.

| Chức năng | Raspberry Pi 3 (40 chân) | Generic ESP32 thường |
|---|---|---|
| SDA | BCM GPIO2, chân vật lý 3 | GPIO21 |
| SCL | BCM GPIO3, chân vật lý 5 | GPIO22 |
| Nguồn | 3V3, chân vật lý 1 | Chân nhãn 3V3 |
| GND | Chân vật lý 6 | Chân nhãn GND |
| Cảm biến 1/trái | 0x23, ADDR thấp | 0x23, ADDR thấp |
| Cảm biến 2/phải | 0x5C, ADDR cao | 0x5C, ADDR cao |

GPIO numbering và chân vật lý của Pi là hai cách đánh số khác nhau. Các vị
trí header của ESP32 thay đổi theo board, nên không đưa số chân vật lý giả.
Nếu dùng ESP32-S2/S3/C3 hoặc board khác, cần cấu hình theo pinout của board đó.
GPIO21/22 là cấu hình của Generic ESP32 thường, không áp dụng mọi họ ESP32.
Driver light_sensor.c dùng cấu hình Pi3 để chọn /dev/i2c-1 và địa chỉ I2C.
Khai báo chân ESP32 không biến driver Linux thành driver ESP-IDF/Arduino.

Hai BH1750 dùng chung SDA/SCL/GND, địa chỉ khác nhau. Kiểm tra ADDR/jumper
trên module. Với board logic 3.3V, giữ SDA/SCL ở 3.3V phù hợp module; không
đưa 5V trực tiếp vào GPIO. Trên Raspberry Pi có thể chạy `pinout` để đối chiếu
header trước khi nối dây. Lệnh này chỉ hiển thị pinout, không dò dây đã nối.

Nguồn pinout:
- https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#gpio-and-the-40-pin-header
- https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html

## Chạy sau khi giải nén

Thư mục standalone chứa ba module và các dependency C/header cần thiết:

```bash
cd standalone
cmake -S . -B build
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/light_demo --sim
./build/light_demo --pins rpi3
./build/light_demo --pins esp32
```

Đọc sensor thật trên Linux: ./build/light_demo --i2c. Demo số lux nhập tay:
./build/light_demo 400 600 vẫn báo chưa có góc hiệu chuẩn; chỉ --sim sử dụng
bảng ảo. Trong repo, thay `cmake -S .` bằng `cmake -S tools/light_demo` và
đặt build directory riêng như build-light-unit.

Gói có full_source/ chứa các source đã chỉnh theo đường dẫn repo, và
light_three_modules_upgrade_v2.patch cho bản SHADOW_V2 đã gửi trước. Kiểm tra
trước khi áp dụng từ thư mục gốc repo:

```bash
git apply --check /duong/dan/light_three_modules_upgrade_v2.patch
git apply /duong/dan/light_three_modules_upgrade_v2.patch
```

Nếu đang dùng đúng repo nền chưa thêm LIGHT_APP V2, dùng patch
light_three_modules_from_base.patch (commit nền 91ae348df26ef91388310a43a26edcc18d3f7702).
Không dùng cả hai patch. Nếu có local edits khiến check thất bại, đối chiếu
thay đổi thay vì ghi đè. Chi tiết build cFS vẫn theo profile light_demo_defs
và hướng dẫn LIGHT_APP_SHADOW_VI.md, nhưng mô phỏng hiện đã có góc tham chiếu.

Đã kiểm tra: build C99; bốn test trong repo; 160 mẫu góc ảo ở hai chu kỳ;
driver I2C giả lập; ADCS freshness/validity; cấu hình chân; build cFS native
non-EDS. Không có phần cứng thật trong lần kiểm tra này. Chưa thêm PID/ESC.
