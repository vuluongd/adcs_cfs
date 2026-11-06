# ADCS Hardware Test README

## 1. Mục tiêu

Tài liệu này hướng dẫn kiểm tra toàn bộ chuỗi ADCS trên **phần cứng thật**:

`light_app` (cảm biến ánh sáng) + `imu_app` (IMU) → `adcs_app` (điều khiển) → `act_app` (bánh đà)

Mục tiêu xác nhận:
- Các app khởi động ổn định
- Cảm biến đọc dữ liệu thật đúng
- Bánh đà nhận lệnh và phản hồi đúng
- Vòng điều khiển ADCS phản ứng đúng hướng nguồn sáng
- Trạng thái an toàn (disable / stop) hoạt động đúng

---

## 2. Phạm vi kiểm tra

### Trong phạm vi
- Kiểm tra tích hợp phần cứng thật
- Kiểm tra telemetry / command
- Kiểm tra chiều quay và phản xạ điều khiển
- Kiểm tra dừng an toàn

### Ngoài phạm vi
- Unit test trên host
- Mô phỏng sensor / simulation backend
- Tối ưu tham số điều khiển dài hạn

---

## 3. Phần cứng yêu cầu

- Board chạy cFS (ví dụ Raspberry Pi hoặc target tương đương)
- 2 cảm biến ánh sáng I2C (trái / phải)
- IMU MPU6050
- Bánh đà phản lực + driver tương thích `act_app`
- Nguồn điện ổn định
- Máy ground để gửi lệnh và xem telemetry

### Kiểm tra kết nối trước khi chạy

- [ ] Cảm biến ánh sáng trái/phải nối đúng bus I2C
- [ ] IMU nối đúng bus I2C
- [ ] Bánh đà / driver có nguồn và tín hiệu điều khiển
- [ ] Board đã nhận đủ quyền truy cập I2C
- [ ] Cơ cấu bánh đà không bị kẹt cơ khí

```bash
ls -l /dev/i2c-*
i2cdetect -y 1
```

> Chỉnh số bus (`1`, `0`, ...) theo phần cứng thực tế.

---

## 4. Cấu hình môi trường phần cứng

Chỉ dùng backend phần cứng thật:

```bash
export LIGHT_APP_BACKEND=i2c
export LIGHT_APP_I2C_DEVICE=/dev/i2c-1
```

Kiểm tra user hiện tại có quyền I2C:

```bash
groups
```

Nếu thiếu group `i2c`:

```bash
sudo usermod -aG i2c $USER
```

Đăng nhập lại để nhận group mới.

---

## 5. Khởi động hệ thống

1. Cấp nguồn phần cứng
2. Chạy cFS (binary/startup script của mission)
3. Mở ground system và kết nối telemetry
4. Kiểm tra log khởi tạo của các app:
   - `imu_app`
   - `light_app`
   - `act_app`
   - `adcs_app`

### Kỳ vọng
- Không có lỗi tạo pipe
- Không có lỗi subscribe MID
- Không có lỗi mở I2C kéo dài
- Housekeeping của từng app nhận được

Nếu app exit hoặc báo lỗi khởi tạo: **dừng test**, xử lý trước khi tiếp tục.

---

## 6. Quy trình test phần cứng

Thực hiện đúng thứ tự dưới đây.

### Bước 1: Kiểm tra sống hệ thống

- [ ] Gửi `NOOP` tới từng app
- [ ] Command counter tăng
- [ ] Nhận HK định kỳ
- [ ] Không có error event liên tục

**PASS:** hệ thống ổn định, telemetry bình thường  
**FAIL:** mất telemetry, app crash, pipe error

---

### Bước 2: Kiểm tra IMU thật

Quan sát `IMU_DATA`:

- [ ] `valid = 1`
- [ ] `status = OK`
- [ ] Khi để yên: `rate_rad_s` gần 0
- [ ] Khi xoay board quanh trục điều khiển: rate đổi rõ
- [ ] `angle_rad` tích phân theo chiều xoay

Ghi nhận:
- Rate lúc đứng yên
- Chiều tăng/giảm của `angle_rad` khi xoay sang phải

**PASS:** dữ liệu IMU hợp lệ, phản ứng đúng chuyển động  
**FAIL:** invalid kéo dài, rate bất thường, không đổi khi xoay

---

### Bước 3: Kiểm tra cảm biến ánh sáng thật

Quan sát `LIGHT_DATA`:

- [ ] `left_lux` và `right_lux` > 0 khi có ánh sáng
- [ ] `valid = 1`
- [ ] Không kẹt trạng thái IO error

Thử hướng sáng:
1. Chiếu sáng lệch trái
2. Chiếu sáng lệch phải
3. Chiếu gần trung tâm

Kỳ vọng:
- Bearing đổi dấu theo phía sáng
- Bearing gần 0 khi sáng đều / trung tâm

Ghi nhận:
- Lux trái / phải
- Bearing tương ứng

**PASS:** bearing phản ánh đúng hướng sáng  
**FAIL:** lux = 0 liên tục, bearing không đổi, lỗi I2C

---

### Bước 4: Kiểm tra bánh đà độc lập (chưa bật ADCS)

Chỉ test `act_app`:

1. Gửi tốc độ thấp theo chiều dương
2. Gửi tốc độ thấp theo chiều âm
3. Gửi lệnh dừng

Checklist:
- [ ] Bánh đà quay đúng khi có lệnh
- [ ] Đổi chiều khi đổi dấu tốc độ
- [ ] Dừng khi nhận lệnh stop
- [ ] `meas_omega_rad_s` bám theo lệnh trong giới hạn an toàn

**Lưu ý an toàn**
- Bắt đầu tốc độ thấp
- Không đứng sát mặt phẳng quay
- Sẵn sàng gửi stop

**PASS:** điều khiển bánh đà thủ công ổn định  
**FAIL:** không quay, không dừng, đo tốc độ sai hoàn toàn

---

### Bước 5: Kiểm tra vòng điều khiển ADCS

#### 5.1. Điều kiện đầu vào
- IMU usable
- Light usable
- ACT đã kiểm tra độc lập OK
- Mode ADCS đang DISABLED

#### 5.2. Bật mode tracking
- Gửi lệnh bật mode tracking (mode = 1)
- Quan sát state điều khiển và omega command

Checklist:
- [ ] State chuyển giữa `LIGHT` / `IMU_HOLD` / `LOST` hợp lý
- [ ] `ErrorRad` thay đổi theo bearing
- [ ] `OmegaCmdRadS` thay đổi theo sai số
- [ ] Bánh đà quay để giảm sai số hướng sáng

#### 5.3. Kịch bản hành vi
1. Đưa nguồn sáng lệch phải  
   - Hệ thống phải phản ứng quay theo hướng giảm sai số
2. Đưa nguồn sáng lệch trái  
   - Hệ thống quay ngược lại
3. Giữ nguồn sáng trung tâm  
   - Sai số nhỏ, lệnh bánh đà giảm / về gần 0
4. Che nguồn sáng tạm thời  
   - Có thể sang `IMU_HOLD`, sau đó `LOST` và dừng nếu mất dữ liệu tin cậy

#### 5.4. Tắt mode
- Chuyển mode về DISABLED
- [ ] Bánh đà dừng
- [ ] Không còn phát lệnh tốc độ mới

**PASS:** phản xạ đúng hướng, dừng an toàn khi disable/mất dữ liệu  
**FAIL:** đảo chiều điều khiển, không phản ứng, không dừng

---

## 7. Bảng ghi kết quả test

| Hạng mục | Kết quả đo | Pass/Fail | Ghi chú |
|----------|------------|-----------|---------|
| Khởi động app |  |  |  |
| IMU valid/status |  |  |  |
| Light lux L/R |  |  |  |
| Bearing trái/phải/tâm |  |  |  |
| ACT quay dương |  |  |  |
| ACT quay âm |  |  |  |
| ACT stop |  |  |  |
| ADCS mode tracking |  |  |  |
| Phản xạ theo ánh sáng |  |  |  |
| Disable/stop an toàn |  |  |  |

---

## 8. Kiểm tra dấu chiều quay

Hai tham số quan trọng trong `adcs_app_control.h`:
- `wheel_sign`
- `imu_right_sign`

### Cách xác nhận
1. Xác định bearing dương = ánh sáng bên phải
2. Quan sát chiều quay bánh đà khi error dương
3. Quan sát IMU angle tăng hay giảm khi thân quay sang phải

Nếu hệ thống quay **ngược** hướng cần chỉnh:
- Chỉ đổi **một** dấu tại một thời điểm
- Test lại ngay kịch bản lệch phải / lệch trái

---

## 9. Tiêu chí PASS tổng

Hệ thống đạt khi đồng thời thỏa:

- [ ] Tất cả app chạy ổn định trên phần cứng
- [ ] IMU và light cho dữ liệu hợp lệ
- [ ] Điều khiển bánh đà thủ công đúng
- [ ] ADCS phản ứng đúng hướng nguồn sáng
- [ ] Disable / mất dữ liệu tin cậy dẫn tới dừng bánh đà
- [ ] Không có lỗi I/O hoặc crash trong phiên test

---

## 10. Sự cố thường gặp và xử lý

| Hiện tượng | Nguyên nhân có thể | Cách xử lý |
|------------|--------------------|------------|
| Không có LIGHT_DATA | Sai bus I2C, sai địa chỉ, thiếu quyền | Kiểm tra `i2cdetect`, wiring, group `i2c` |
| IMU invalid | Lỗi kết nối, nhiễu, cảm biến chưa sẵn sàng | Kiểm tra dây, nguồn, log status |
| Bánh đà không quay | Driver, nguồn, dấu lệnh, cơ khí | Test ACT độc lập, đo tín hiệu điều khiển |
| Quay ngược chiều | Sai `wheel_sign` hoặc `imu_right_sign` | Đổi từng dấu và test lại |
| ADCS không ra lệnh | Mode DISABLED, sensor chưa usable | Kiểm tra mode + cờ usable |
| App thoát bất thường | Lỗi pipe/runtime | Xem log event cuối trước khi exit |

---

## 11. An toàn bắt buộc

- Luôn test bánh đà ở tốc độ thấp trước
- Không bật tracking khi chưa xác nhận chiều quay
- Không rời vị trí khi mode tracking đang bật
- Khi có bất thường: gửi stop / chuyển DISABLED ngay
- Cắt nguồn nếu bánh đà không dừng theo lệnh

---

## 12. Kết thúc phiên test

Trước khi tắt hệ thống:

1. Chuyển ADCS về DISABLED
2. Gửi `STOP_WHEEL`
3. Xác nhận bánh đà đã dừng hoàn toàn
4. Lưu log / telemetry quan trọng
5. Ghi kết quả vào bảng test
6. Tắt ground system và cắt nguồn theo quy trình

---

## 13. Kết luận phiên test

- Ngày test:
- Người test:
- Phần cứng / board:
- Kết quả tổng: PASS / FAIL
- Vấn đề còn lại:
- Hành động tiếp theo:

---

**Ghi chú:** Tài liệu này chỉ dùng cho kiểm tra phần cứng thật. Mọi bước đều giả định cảm biến, IMU và bánh đà đang kết nối vật lý đầy đủ.
```