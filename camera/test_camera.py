"""
Test camera_logger.py
Chạy: python test_camera.py

Cần cài OpenCV trước:
    pip install opencv-python
"""

import time
import math
import random
from camera_logger import CameraLogger, CameraDataLogger
from data_logger import IMUData, LightSensorData, ADCSState


# ══════════════════════════════════════════════
# Test 1: Chụp ảnh thủ công
# ══════════════════════════════════════════════
def test_manual_capture():
    print("\n" + "="*50)
    print("TEST 1 — Chụp ảnh thủ công (3 ảnh)")
    print("="*50)

    cam = CameraLogger()
    cam.start()

    for i in range(3):
        path = cam.capture_photo(label=f"manual_{i+1}")
        print(f"  Anh {i+1}: {path}")
        time.sleep(1)

    cam.stop()


# ══════════════════════════════════════════════
# Test 2: Chụp tự động background
# ══════════════════════════════════════════════
def test_auto_capture():
    print("\n" + "="*50)
    print("TEST 2 — Auto capture 2s/anh trong 8 giay")
    print("="*50)

    cam = CameraLogger()
    cam.start()
    cam.start_auto_capture(interval_s=2.0, label="auto")

    print("  Dang chup tu dong...")
    time.sleep(8)

    cam.stop_auto_capture()
    cam.stop()


# ══════════════════════════════════════════════
# Test 3: Ghép Camera + DataLogger
# ══════════════════════════════════════════════
def test_combined():
    print("\n" + "="*50)
    print("TEST 3 — Camera + DataLogger dong bo (10 chu ky)")
    print("="*50)

    t0 = time.monotonic()

    with CameraDataLogger() as system:
        for i in range(10):
            t = time.monotonic() - t0

            imu = IMUData(
                accel_z = round(9.81 + random.gauss(0, 0.01), 4),
                gyro_z  = round(0.1 * math.sin(t), 5),
                temp    = 36.0,
            )
            light = LightSensorData(lux_0=round(300 + 50*math.sin(t), 1))
            state = ADCSState(
                yaw        = round(t % 6.28, 3),
                wheel_rpm  = round(1000 + 200*math.sin(t), 1),
                control_mode = "POINTING",
            )

            # Chụp ảnh mỗi 3 chu kỳ
            should_capture = (i % 3 == 0)
            system.record(imu=imu, light=light, state=state, capture=should_capture)

            print(f"  [{i+1:02d}] CSV ghi + {'anh chup' if should_capture else 'bo qua anh'}")
            time.sleep(0.5)


# ══════════════════════════════════════════════
if __name__ == "__main__":
    test_manual_capture()
    test_auto_capture()
    test_combined()

    print("\n" + "="*50)
    print("Tat ca test xong! Kiem tra thu muc ./captures/")
    print("="*50)
