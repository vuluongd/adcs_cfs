# ADCS Unified Ground Control — Một giao diện trong NASA cFS-GroundSystem

Đây là **bản thay giao diện** để gửi command và xem telemetry ngay trong **một cửa sổ PyQt5**. Không tạo web, không chạy server Python v5, không cần mở `GroundSystem.py` cũ. Các thành phần NASA `RoutingService.py`, ZeroMQ và binary `Subsystems/cmdUtil/cmdUtil` **vẫn được tái sử dụng**.

Nguồn đối chiếu: bản `adcs_cfs-main.zip` đã cung cấp, snapshot 30/10/2026, thư mục `tools/cFS-GroundSystem` và các header `apps/imu_app`, `apps/act_app`, `apps/light_app`. Nếu code GitHub đã thay đổi, cần đối chiếu lại MID/CC, định nghĩa packet và cấu hình mission.

## 1. Cài bản mới từ Windows vào VirtualBox Ubuntu

Trên Windows, copy file `cfs-groundsystem-unified-ui.zip` vào thư mục Shared Folder bạn đã cấu hình: `C:\Users\User\Downloads\adcs-nasa-operator-v5-full`.

Trong terminal Ubuntu:

```bash
sudo apt update
sudo apt install -y unzip python3-pyqt5 python3-zmq build-essential
mkdir -p ~/ground-ui-upgrade
unzip /media/sf_adcs-nasa-operator-v5-full/cfs-groundsystem-unified-ui.zip -d ~/ground-ui-upgrade
cd ~/ground-ui-upgrade/cFS-GroundSystem-UI
bash install_ui.sh
```

Script chỉ thêm ba file `GroundControl.py`, `unified_logic.py` và `test_unified_logic.py` vào `~/adcs_cfs/tools/cFS-GroundSystem`; tự sao lưu khi trùng tên. **Không ghi đè source cFS, GroundSystem.py hay RoutingService.py cũ.**

Nếu thư mục gốc nằm ở nơi khác, sử dụng `bash install_ui.sh /duong/dan/cFS-GroundSystem`.

## 2. Chạy ứng dụng UI duy nhất

Hãy đóng cửa sổ **NASA GroundSystem cũ** và tắt backend web v5 (nếu đang chạy). Một lúc chỉ có một tiến trình nhận UDP TLM `:2234`.

Giữ cFS chạy trên Ubuntu ở terminal khác, rồi:

```bash
cd ~/adcs_cfs/tools/cFS-GroundSystem
make -C Subsystems/cmdUtil
python3 GroundControl.py
```

**Không chạy `python3 GroundSystem.py`** khi bạn muốn dùng UI mới. Cửa sổ mới có cùng lúc ô địa chỉ IP, bảng packet, gửi lệnh, biểu đồ Z, chi tiết packet, nhật ký. Không yêu cầu Vercel/Supabase.

## 3. Điền IP cho trường hợp hiện tại: cFS + Ground Control cùng một Ubuntu VM

| Ô trên giao diện | Giá trị |
|---|---|
| IP cFS / Raspberry Pi | `127.0.0.1` |
| UDP Command | `1234` |
| UDP Telemetry (máy trạm) | `2234` (mặc định theo `RoutingService.py` của source) |
| IP máy trạm nhận TLM | `127.0.0.1` |

Thao tác:

1. Bấm **Bắt đầu nhận telemetry** để mở UDP trên Ubuntu và khởi động NASA RoutingService + ZeroMQ **ngay trong ứng dụng mới**.
2. Bấm **Bật TLM trên cFS**. Xác nhận để gửi lệnh `TO_LAB` `MID 0x1880`, `CC 6` tới cFS. Đây là yêu cầu, chưa phải xác nhận cFS đã thực thi.
3. Đợi bảng **Telemetry** cập nhật số gói, `0x0808`, `0x0800`... nếu các packet đó được cấu hình TO_LAB phát về trạm.
4. Nếu cFS đã build `imu_app`, `light_app`, `act_app` và có nguồn dữ liệu, sẽ xuất hiện `0x08D4`, `0x08DC`, `0x08E4`.
5. Ở panel **Gửi lệnh**, chọn ứng dụng và lệnh rồi nhấn **Gửi lệnh**. `NOOP` dùng để thử đầu tiên.

**Quan trọng:** Header của một số bản cFS có phần mở rộng. Bạn có thể đổi `Telemetry header offset bổ sung` từ **0** sang **4** nếu trường giải mã lệch. Số packet và raw hex vẫn hiện ngay cả khi chưa có định nghĩa cho MID đó.

## 4. Khi có Raspberry Pi 3

Cấu hình VirtualBox `Network → Bridged Adapter`, xác nhận Ubuntu có thể kết nối Raspberry Pi. Thay `127.0.0.1` ở ô **IP cFS** bằng IP Pi, ví dụ `192.168.1.50`; ô **IP máy trạm nhận TLM** phải là IP máy ảo Ubuntu thực tế, ví dụ `192.168.1.100`. Chuyển `UDP Command`/`UDP Telemetry` nếu cFS của bạn cấu hình cổng khác; không nhất thiết là cổng trong hình slide.

Không dùng địa chỉ `127.0.0.1` làm telemetry destination khi cFS chạy trên Pi còn Ground Control chạy trên máy ảo riêng.

## 5. Các chức năng được gom lại

- **Command:** NOOP, RESET_COUNTERS một số app, IMU RECALIBRATE/RESET_ANGLE, ACT SET_WHEEL_SPEED/STOP_WHEEL, TO_LAB ENABLE_TLM.
- **Telemetry:** bảng packet ID + bộ đếm + thời gian nhận, tự nhận dạng các packet, giải mã theo `Subsystems/tlmGUI/*-tlm.txt`, hiển thị raw hex.
- **ADCS trục Z:** góc, tốc độ góc, tốc độ bánh đà, đồ thị góc theo thời gian, ước lượng độ lệch chuẩn gyro khi đang đứng yên.
- **DEMO:** tạo mẫu mô phỏng trong chính UI; **không phát lệnh tới cFS**.

### Các giới hạn cố ý

- `SET_WHEEL_SPEED` chỉ trong phạm vi **±60 rad/s**, phải có telemetry ACT thật mới trong 5 giây, `Valid=1`, `Status=0`, `Simulated=0`, bật ARM 60 giây và xác nhận gửi. Lệnh STOP WHEEL cần xác nhận nhưng không cần ARM; không thay thế ngắt nguồn khẩn cấp.
- IMU RECALIBRATE/RESET_ANGLE cũng cần ARM và telemetry IMU phù hợp. `NOOP` không cần ARM.
- Không điều khiển giữ góc Z tự động trong LIVE; source chưa có command/controller đã được xác minh cho tính năng này.
- Phiên bản tối giản **không tích hợp FDL/FUL, upload bảng và các command có payload tùy ý**. Các phần đó vẫn còn nguyên trong NASA GroundSystem cũ để xử lý độc lập nếu cần.
- **Gửi thành công qua UDP chỉ có nghĩa là `SENT_UNCONFIRMED`**, không coi là ACK hay thực thi thành công. Phải kiểm tra telemetry, command counters, event logs.
- Dùng trong lab; chưa kiểm thử với thiết bị thật của bạn.

## 6. Khắc phục lỗi

**Không mở được UI:**

```bash
cd ~/adcs_cfs/tools/cFS-GroundSystem
python3 GroundControl.py
```

Nếu báo `No module named PyQt5` / `zmq`, cài `sudo apt install -y python3-pyqt5 python3-zmq`.

**UDP đang bận:** Tắt `GroundSystem.py` cũ và ứng dụng nhận cổng 2234 khác. Có thể kiểm tra:

```bash
sudo ss -lunp | grep -E ':2234|:1234'
```

**TLM = 0 packet:** kiểm tra `TO_LAB` có chạy và gửi về đúng máy trạm/cổng. Đừng nhầm UDP 1234 (command) với 2234 (telemetry mặc định source). Có thể dùng:

```bash
sudo apt install -y tcpdump
sudo tcpdump -ni any udp port 2234 -c 5
```

**cmdUtil chưa build:**

```bash
cd ~/adcs_cfs/tools/cFS-GroundSystem
make -C Subsystems/cmdUtil
```

**Test logic/decoder/command formats:**

```bash
cd ~/adcs_cfs/tools/cFS-GroundSystem
python3 -m unittest -v test_unified_logic.py
```

## 7. Tài liệu nguồn

- NASA: https://github.com/nasa/cFS-GroundSystem
- Dự án ADCS: https://github.com/vuluongd/adcs_cfs/tree/main/tools/cFS-GroundSystem

Bản mới giữ nguyên kiến trúc **cFS ↔ UDP ↔ NASA RoutingService/ZeroMQ ↔ giao diện PyQt5**; khác biệt nằm ở việc người vận hành chỉ cần một cửa sổ.
