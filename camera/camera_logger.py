import cv2
import os
import time
import threading
from datetime import datetime
from pathlib import Path
from data_logger import DataLogger, IMUData, LightSensorData, ADCSState
CAMERA_SOURCE = "libcamerasrc ! video/x-raw,width=640,height=480 ! videoconvert ! appsink"

SAVE_DIR = Path("/images")
FRAME_WIDTH  = 640
FRAME_HEIGHT = 480
CAPTURE_FPS  = 5  


def open_camera() -> cv2.VideoCapture:
    cap = cv2.VideoCapture(CAMERA_SOURCE)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  FRAME_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT)
    cap.set(cv2.CAP_PROP_FPS, CAPTURE_FPS)
    return cap
class CameraLogger:

    def __init__(self, save_dir: Path = SAVE_DIR):
        self.save_dir = Path(save_dir)
        self.save_dir.mkdir(parents=True, exist_ok=True)

        self._cap       = None
        self._running   = False
        self._auto_thread = None
        self._stop_evt  = threading.Event()

        self._photo_count = 0
        self._session_dir = None
    def start(self):
        session_name = datetime.now().strftime("session_%Y%m%d_%H%M%S")
        self._session_dir = self.save_dir / session_name
        self._session_dir.mkdir(parents=True, exist_ok=True)

        self._cap = open_camera()

        if not self._cap.isOpened():
            raise RuntimeError(
                "Không mở được camera!\n"
                "→ Kiểm tra: camera có được cắm không?\n"
                "→ Thử đổi CAMERA_SOURCE = 1 hoặc 2"
            )

        self._running     = True
        self._photo_count = 0
        print(f"[Camera] Đã mở camera → lưu vào: {self._session_dir}")

    def stop(self):
        """Dừng camera và giải phóng tài nguyên."""
        self._stop_evt.set()
        self._running = False

        if self._auto_thread and self._auto_thread.is_alive():
            self._auto_thread.join(timeout=3)

        if self._cap:
            self._cap.release()
            self._cap = None

        print(f"[Camera] Dừng — đã chụp {self._photo_count} ảnh")


    def capture_photo(self, label: str = "") -> str | None:
        if not self._running or not self._cap:
            print("[Camera] Chưa start camera!")
            return None

        ret, frame = self._cap.read()
        if not ret:
            print("[Camera] ❌ Không đọc được frame!")
            return None

        # Đặt tên file: photo_000001_20261028_150000_label.jpg
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]
        label_str = f"_{label}" if label else ""
        filename  = f"photo_{self._photo_count:06d}_{timestamp}{label_str}.jpg"
        filepath  = self._session_dir / filename

        # [ĐỔI KHI LÊN PI — nếu ảnh bị lật ngược/xoay]:
        # Bỏ comment dòng phù hợp:
        # frame = cv2.flip(frame, 0)   # lật dọc
        # frame = cv2.flip(frame, 1)   # lật ngang
        # frame = cv2.rotate(frame, cv2.ROTATE_90_CLOCKWISE)

        cv2.imwrite(str(filepath), frame)
        self._photo_count += 1
        print(f"[Camera] 📸 {filename}")
        return str(filepath)

    def read_frame(self):
        """Đọc 1 frame (không lưu) — dùng để preview hoặc xử lý."""
        if not self._cap:
            return None
        ret, frame = self._cap.read()
        return frame if ret else None

    # ── Chụp tự động ────────────────────────────────────────

    def start_auto_capture(self, interval_s: float = 2.0, label: str = "auto"):
        self._stop_evt.clear()
        self._auto_thread = threading.Thread(
            target=self._auto_loop,
            args=(interval_s, label),
            daemon=True,
            name="CameraAutoCapture"
        )
        self._auto_thread.start()
        print(f"[Camera] ▶ Auto capture mỗi {interval_s}s")

    def stop_auto_capture(self):
        self._stop_evt.set()
        print("[Camera] ⏹ Dừng auto capture")

    def _auto_loop(self, interval_s: float, label: str):
        while not self._stop_evt.is_set() and self._running:
            t0 = time.monotonic()
            self.capture_photo(label=label)
            elapsed = time.monotonic() - t0
            time.sleep(max(0.0, interval_s - elapsed))

class CameraDataLogger:
    def __init__(self):
        self.cam    = CameraLogger()
        self.logger = DataLogger()

    def start(self):
        self.cam.start()
        file_path = self.logger.start()
        print(f"[System] Bắt đầu ghi đồng bộ → CSV: {file_path}")

    def record(
        self,
        imu:     IMUData         = None,
        light:   LightSensorData = None,
        state:   ADCSState       = None,
        capture: bool            = False,
        label:   str             = ""
    ):
        self.logger.log(imu=imu, light=light, state=state)

        if capture:
            self.cam.capture_photo(label=label or (state.control_mode if state else ""))

    def stop(self):
        self.cam.stop()
        self.logger.stop()

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *_):
        self.stop()
