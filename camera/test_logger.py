"""
Test data_logger.py — chạy file này để kiểm tra hoạt động
Không cần Raspberry Pi, chạy ngay trên máy tính cá nhân.

Chạy: python test_logger.py
"""

import time
import math
import random
from data_logger import DataLogger, AutoLogger, IMUData, LightSensorData, ADCSState


# ══════════════════════════════════════════════
# Test 1: Dùng DataLogger thủ công (manual)
# ══════════════════════════════════════════════
def test_manual_logging():
    print("\n" + "═"*50)
    print("TEST 1 — Manual Logging (20 dòng)")
    print("═"*50)

    with DataLogger() as logger:
        for i in range(20):
            t = i * 0.1

            # Giả lập dữ liệu IMU (sin wave cho gyro)
            imu = IMUData(
                accel_x = round(random.gauss(0, 0.05), 4),
                accel_y = round(random.gauss(0, 0.05), 4),
                accel_z = round(9.81 + random.gauss(0, 0.02), 4),
                gyro_x  = round(0.01 * math.sin(t), 5),
                gyro_y  = round(0.01 * math.cos(t), 5),
                gyro_z  = round(0.05 * math.sin(0.5 * t), 5),
                temp    = round(35.0 + random.gauss(0, 0.1), 2),
            )

            # Giả lập cảm biến ánh sáng
            light = LightSensorData(
                lux_0   = round(300 + 50 * math.sin(t), 1),
                lux_1   = round(280 + 50 * math.cos(t), 1),
                lux_2   = round(310 + random.gauss(0, 5), 1),
                raw_adc = random.randint(2000, 2100),
            )

            # Giả lập trạng thái ADCS
            state = ADCSState(
                roll        = round(0.1 * math.sin(t), 4),
                pitch       = round(0.05 * math.cos(t), 4),
                yaw         = round(0.2 * t % (2 * math.pi), 4),
                wheel_rpm   = round(1000 + 200 * math.sin(t), 1),
                motor_duty  = round(30 + 10 * math.sin(t), 1),
                control_mode= "POINTING" if i > 5 else "DETUMBLE",
            )

            ok = logger.log(imu, light, state)
            print(f"  [{i+1:02d}] Ghi {'✓' if ok else '✗'} | yaw={state.yaw:.3f} rad | rpm={state.wheel_rpm:.0f}")
            time.sleep(0.05)

        stats = logger.get_stats()
        print(f"\n  📊 Tổng kết: {stats['rows']} dòng trong {stats['elapsed_s']}s")
        print(f"  📁 File: {stats['file']}")


# ══════════════════════════════════════════════
# Test 2: AutoLogger chạy background (tự động)
# ══════════════════════════════════════════════
def test_auto_logging():
    print("\n" + "═"*50)
    print("TEST 2 — Auto Logging ở background (5 giây, 10Hz)")
    print("═"*50)

    t0 = time.monotonic()

    def read_imu():
        t = time.monotonic() - t0
        return IMUData(
            accel_z = round(9.81 + random.gauss(0, 0.01), 4),
            gyro_z  = round(0.1 * math.sin(t), 5),
            temp    = round(36.0 + random.gauss(0, 0.05), 2),
        )

    def read_light():
        t = time.monotonic() - t0
        return LightSensorData(
            lux_0 = round(300 + 80 * math.sin(t), 1),
            lux_1 = round(280 + 80 * math.cos(t), 1),
        )

    def read_state():
        t = time.monotonic() - t0
        return ADCSState(
            yaw         = round(t % (2 * math.pi), 4),
            wheel_rpm   = round(1500 + 300 * math.sin(t), 1),
            control_mode= "POINTING",
        )

    auto = AutoLogger(read_imu, read_light, read_state, rate_hz=10)
    path = auto.start()
    print(f"  ▶ AutoLogger đang chạy... (ghi vào {path})")

    # Chương trình chính có thể làm việc khác trong khi logger chạy nền
    for i in range(5):
        time.sleep(1)
        stats = auto.get_stats()
        print(f"  [{i+1}s] {stats['rows']} dòng | {stats['avg_hz']} Hz")

    auto.stop()
    print("  ⏹ AutoLogger dừng.")


# ══════════════════════════════════════════════
# Test 3: Ghi lỗi (mô phỏng sensor bị lỗi)
# ══════════════════════════════════════════════
def test_error_logging():
    print("\n" + "═"*50)
    print("TEST 3 — Ghi khi sensor bị lỗi")
    print("═"*50)

    with DataLogger() as logger:
        # Dòng bình thường
        logger.log(imu=IMUData(accel_z=9.81))
        print("  ✓ Dòng bình thường")

        # IMU bị lỗi — truyền None
        logger.log(imu=None, light=LightSensorData(lux_0=300), error="IMU timeout")
        print("  ✓ Dòng IMU lỗi (None)")

        # Cả 2 sensor lỗi
        logger.log(error="Mất kết nối I2C bus")
        print("  ✓ Dòng mất kết nối")

        print(f"  📁 File: {logger.get_stats()['file']}")


# ══════════════════════════════════════════════
if __name__ == "__main__":
    test_manual_logging()
    test_auto_logging()
    test_error_logging()

    print("\n" + "═"*50)
    print("✅ Tất cả test PASS! Kiểm tra thư mục ./logs/ để xem file CSV.")
    print("═"*50)
