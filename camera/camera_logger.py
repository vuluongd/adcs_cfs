"""
ADCS 1-DOF — Camera Logger Module
===================================
Chụp ảnh / quay video và lưu vào SD card, ghép với DataLogger.

Người thực hiện : Hoàn
Dự án           : ADCS 1-DOF — Nhóm 2

──────────────────────────────────────────────────────────────
HƯỚNG DẪN ĐỔI MÔI TRƯỜNG
──────────────────────────────────────────────────────────────
Tìm các dòng có tag [ĐỔI KHI LÊN PI] và làm theo hướng dẫn.
Tất cả chỗ khác GIỮ NGUYÊN, không cần sửa.
──────────────────────────────────────────────────────────────
"""

import cv2
import os
import time
import threading
from datetime import datetime
from pathlib import Path

# ── Import DataLogger đã viết tuần trước ─────────────────────
from data_logger import DataLogger, IMUData, LightSensorData, ADCSState


# ══════════════════════════════════════════════════════════════
# [ĐỔI KHI LÊN PI] — CHỌN LOẠI CAMERA
# ══════════════════════════════════════════════════════════════
#
#  ĐANG DÙNG (Windows - webcam máy tính):
#    CAMERA_SOURCE = 0
#
#  Khi lên Raspberry Pi, đổi thành 1 trong 3 cách sau:
#
#  • USB webcam trên Pi:
#    CAMERA_SOURCE = 0          ← giữ nguyên, thường vẫn là 0
#
#  • CSI ribbon camera (Pi Camera Module):
#    CAMERA_SOURCE = 0          ← với OpenCV thường vẫn là 0
#    (nếu không được thì thử: CAMERA_SOURCE = "libcamerasrc ! ...")
#    → xem hướng dẫn chi tiết ở hàm open_camera() bên dưới
#
CAMERA_SOURCE = 0
# ══════════════════════════════════════════════════════════════


# ══════════════════════════════════════════════════════════════
# [ĐỔI KHI LÊN PI] — THƯ MỤC LƯU ẢNH / VIDEO
# ══════════════════════════════════════════════════════════════
#
#  ĐANG DÙNG (Windows):
#    SAVE_DIR = Path("./captures")
#
#  Khi lên Raspberry Pi, đổi thành:
#    SAVE_DIR = Path("/home/pi/captures")   ← lưu vào SD card
#
SAVE_DIR = Path("./captures")
# ══════════════════════════════════════════════════════════════


# ── Cài đặt chất lượng ảnh ───────────────────────────────────
FRAME_WIDTH  = 640
FRAME_HEIGHT = 480
CAPTURE_FPS  = 5      # Chụp 5 ảnh/giây khi ở chế độ auto


def open_camera() -> cv2.VideoCapture:
    """
    Mở kết nối camera.

    [ĐỔI KHI LÊN PI — CSI camera]:
    Nếu dùng Pi Camera Module (CSI) mà OpenCV không nhận,
    thay toàn bộ hàm này bằng:

        from picamera2 import Picamera2    # pip install picamera2
        cam = Picamera2()
        cam.configure(cam.create_preview_configuration(
            main={"size": (640, 480)}
        ))
        cam.start()
        return cam   # rồi sửa các hàm bên dưới dùng cam.capture_array()

    Hoặc dùng GStreamer pipeline với OpenCV:
        pipeline = (
            "libcamerasrc ! video/x-raw,width=640,height=480 ! "
            "videoconvert ! appsink"
        )
        cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)
        return cap
    """
    cap = cv2.VideoCapture(CAMERA_SOURCE)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  FRAME_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT)
    cap.set(cv2.CAP_PROP_FPS, CAPTURE_FPS)
    return cap


# ══════════════════════════════════════════════════════════════
# CameraLogger — chụp ảnh và lưu file
# ══════════════════════════════════════════════════════════════

class CameraLogger:
    """
    Chụp ảnh từ camera và lưu vào thư mục captures/.
    Có thể chạy tự động (auto mode) hoặc chụp thủ công.

    Cách dùng cơ bản:
        cam = CameraLogger()
        cam.start()
        cam.capture_photo()        # chụp 1 ảnh thủ công
        cam.start_auto_capture(interval_s=2)  # tự động 2s/ảnh
        cam.stop()
    """

    def __init__(self, save_dir: Path = SAVE_DIR):
        self.save_dir = Path(save_dir)
        self.save_dir.mkdir(parents=True, exist_ok=True)

        self._cap       = None
        self._running   = False
        self._auto_thread = None
        self._stop_evt  = threading.Event()

        self._photo_count = 0
        self._session_dir = None

    # ── Mở / đóng camera ────────────────────────────────────

    def start(self):
        """Mở camera và tạo thư mục session mới."""
        # Tạo thư mục riêng cho mỗi lần chạy theo timestamp
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

    # ── Chụp ảnh ────────────────────────────────────────────

    def capture_photo(self, label: str = "") -> str | None:
        """
        Chụp 1 ảnh và lưu vào thư mục session.

        Args:
            label: Tên thêm vào file (vd: "detumble", "pointing")

        Returns:
            Đường dẫn file ảnh, hoặc None nếu thất bại.
        """
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
        """
        Tự động chụp ảnh mỗi interval_s giây ở background.

        Args:
            interval_s: Khoảng cách giữa 2 lần chụp (giây)
            label: Nhãn đặt vào tên file
        """
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
        """Dừng chế độ chụp tự động (camera vẫn mở)."""
        self._stop_evt.set()
        print("[Camera] ⏹ Dừng auto capture")

    def _auto_loop(self, interval_s: float, label: str):
        while not self._stop_evt.is_set() and self._running:
            t0 = time.monotonic()
            self.capture_photo(label=label)
            elapsed = time.monotonic() - t0
            time.sleep(max(0.0, interval_s - elapsed))


# ══════════════════════════════════════════════════════════════
# CameraDataLogger — Ghép Camera + DataLogger
# ══════════════════════════════════════════════════════════════

class CameraDataLogger:
    """
    Hệ thống ghi đồng bộ: ảnh + dữ liệu cảm biến.
    Mỗi ảnh chụp được ghép timestamp với dòng CSV tương ứng.

    Cách dùng:
        system = CameraDataLogger()
        system.start()
        system.record(imu=..., light=..., state=..., capture=True)
        system.stop()
    """

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
        """
        Ghi 1 chu kỳ: dữ liệu cảm biến vào CSV + (tuỳ chọn) chụp ảnh.

        Args:
            capture: True → chụp ảnh luôn trong chu kỳ này
            label:   Nhãn ảnh (vd: trạng thái ADCS hiện tại)
        """
        # Ghi data vào CSV
        self.logger.log(imu=imu, light=light, state=state)

        # Chụp ảnh nếu được yêu cầu
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
