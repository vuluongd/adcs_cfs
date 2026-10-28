

import csv
import os
import time
import threading
import logging
from datetime import datetime
from dataclasses import dataclass, fields, astuple
from pathlib import Path
from typing import Optional

# ──────────────────────────────────────────────
# Cấu hình đường dẫn log
# ──────────────────────────────────────────────
# Trên Raspberry Pi: đổi thành "/home/pi/logs" hoặc "/mnt/sdcard/logs"
LOG_DIR = Path("/logs")

# Bao nhiêu dòng thì flush (ghi) xuống file một lần
FLUSH_INTERVAL = 10          # dòng
# Tần suất ghi dữ liệu (giây)
LOG_RATE_HZ    = 10          # 10 Hz = 0.1s / lần


# ──────────────────────────────────────────────
# Data Classes — cấu trúc từng loại dữ liệu
# ──────────────────────────────────────────────

@dataclass
class IMUData:
    """Dữ liệu từ IMU (gia tốc + con quay)"""
    accel_x: float = 0.0   # m/s²
    accel_y: float = 0.0
    accel_z: float = 0.0
    gyro_x:  float = 0.0   # rad/s
    gyro_y:  float = 0.0
    gyro_z:  float = 0.0
    temp:    float = 0.0   # °C

@dataclass
class LightSensorData:
    """Dữ liệu từ cảm biến ánh sáng (sun sensor)"""
    lux_0:   float = 0.0   # Sensor 0
    lux_1:   float = 0.0   # Sensor 1
    lux_2:   float = 0.0   # Sensor 2 (nếu có)
    raw_adc: int   = 0     # Giá trị ADC thô

@dataclass
class ADCSState:
    """Trạng thái ADCS: góc + tốc độ bánh đà"""
    roll:         float = 0.0   # rad
    pitch:        float = 0.0
    yaw:          float = 0.0
    wheel_rpm:    float = 0.0   # RPM bánh đà
    motor_duty:   float = 0.0   # % duty cycle ESC
    control_mode: str   = "IDLE"  # IDLE / DETUMBLE / POINTING

@dataclass
class LogEntry:
    """Một dòng log hoàn chỉnh = timestamp + tất cả sensors"""
    timestamp:    str   = ""   # ISO format
    elapsed_s:    float = 0.0  # giây kể từ khi start
    # IMU
    accel_x: float = 0.0
    accel_y: float = 0.0
    accel_z: float = 0.0
    gyro_x:  float = 0.0
    gyro_y:  float = 0.0
    gyro_z:  float = 0.0
    imu_temp: float = 0.0
    # Light sensor
    lux_0:   float = 0.0
    lux_1:   float = 0.0
    lux_2:   float = 0.0
    raw_adc: int   = 0
    # ADCS state
    roll:         float = 0.0
    pitch:        float = 0.0
    yaw:          float = 0.0
    wheel_rpm:    float = 0.0
    motor_duty:   float = 0.0
    control_mode: str   = "IDLE"
    # Chất lượng
    imu_ok:   bool = False
    light_ok: bool = False
    error_msg: str = ""


# ──────────────────────────────────────────────
# DataLogger Class
# ──────────────────────────────────────────────

class DataLogger:
    """
    Logger ghi dữ liệu ADCS ra CSV theo timestamp.

    Cách dùng:
        logger = DataLogger()
        logger.start()

        imu   = IMUData(accel_x=0.1, gyro_z=0.05)
        light = LightSensorData(lux_0=350.0)
        state = ADCSState(yaw=0.12, wheel_rpm=1500)
        logger.log(imu, light, state)

        logger.stop()
    """

    def __init__(self, log_dir: Path = LOG_DIR, rate_hz: float = LOG_RATE_HZ):
        self.log_dir  = Path(log_dir)
        self.rate_hz  = rate_hz
        self._interval = 1.0 / rate_hz

        self._file   : Optional[object] = None
        self._writer : Optional[csv.DictWriter] = None
        self._lock   = threading.Lock()
        self._running = False

        self._start_time : Optional[float] = None
        self._row_count   = 0
        self._current_file_path: Optional[Path] = None

        # Tạo thư mục log nếu chưa có
        self.log_dir.mkdir(parents=True, exist_ok=True)

        # Thiết lập Python logger để in ra console
        logging.basicConfig(
            level=logging.INFO,
            format="[%(asctime)s] %(levelname)s — %(message)s",
            datefmt="%H:%M:%S"
        )
        self._syslog = logging.getLogger("DataLogger")

    # ── Public API ─────────────────────────────

    def start(self) -> Path:
        """Mở file CSV mới và bắt đầu ghi. Trả về đường dẫn file."""
        with self._lock:
            if self._running:
                self._syslog.warning("Logger đang chạy rồi!")
                return self._current_file_path

            self._start_time = time.monotonic()
            file_name = datetime.now().strftime("adcs_log_%Y%m%d_%H%M%S.csv")
            self._current_file_path = self.log_dir / file_name

            self._file   = open(self._current_file_path, "w", newline="", buffering=1)
            fieldnames   = [f.name for f in fields(LogEntry)]
            self._writer = csv.DictWriter(self._file, fieldnames=fieldnames)
            self._writer.writeheader()

            self._running   = True
            self._row_count = 0

        self._syslog.info(f"▶ Bắt đầu ghi log → {self._current_file_path}")
        return self._current_file_path

    def log(
        self,
        imu:   Optional[IMUData]         = None,
        light: Optional[LightSensorData] = None,
        state: Optional[ADCSState]       = None,
        error: str = ""
    ) -> bool:
        """
        Ghi một dòng dữ liệu vào CSV.
        Trả về True nếu thành công.
        """
        if not self._running:
            self._syslog.error("Logger chưa start! Gọi logger.start() trước.")
            return False

        now     = datetime.now().isoformat(timespec="milliseconds")
        elapsed = round(time.monotonic() - self._start_time, 4)

        entry = LogEntry(
            timestamp  = now,
            elapsed_s  = elapsed,
            # IMU
            accel_x  = imu.accel_x  if imu else 0.0,
            accel_y  = imu.accel_y  if imu else 0.0,
            accel_z  = imu.accel_z  if imu else 0.0,
            gyro_x   = imu.gyro_x   if imu else 0.0,
            gyro_y   = imu.gyro_y   if imu else 0.0,
            gyro_z   = imu.gyro_z   if imu else 0.0,
            imu_temp = imu.temp     if imu else 0.0,
            imu_ok   = imu is not None,
            # Light
            lux_0   = light.lux_0   if light else 0.0,
            lux_1   = light.lux_1   if light else 0.0,
            lux_2   = light.lux_2   if light else 0.0,
            raw_adc = light.raw_adc if light else 0,
            light_ok = light is not None,
            # ADCS state
            roll         = state.roll         if state else 0.0,
            pitch        = state.pitch        if state else 0.0,
            yaw          = state.yaw          if state else 0.0,
            wheel_rpm    = state.wheel_rpm    if state else 0.0,
            motor_duty   = state.motor_duty   if state else 0.0,
            control_mode = state.control_mode if state else "IDLE",
            # Error
            error_msg = error,
        )

        with self._lock:
            self._writer.writerow(entry.__dict__)
            self._row_count += 1
            # Flush định kỳ để tránh mất dữ liệu khi mất điện
            if self._row_count % FLUSH_INTERVAL == 0:
                self._file.flush()
                os.fsync(self._file.fileno())

        return True

    def stop(self):
        """Dừng logger, đóng file an toàn."""
        with self._lock:
            if not self._running:
                return
            self._running = False
            if self._file:
                self._file.flush()
                os.fsync(self._file.fileno())
                self._file.close()
                self._file = None

        duration = round(time.monotonic() - self._start_time, 1)
        self._syslog.info(
            f"⏹ Dừng logger — {self._row_count} dòng / {duration}s → {self._current_file_path}"
        )

    def get_stats(self) -> dict:
        """Thống kê nhanh về session ghi hiện tại."""
        elapsed = round(time.monotonic() - self._start_time, 1) if self._start_time else 0
        return {
            "running"    : self._running,
            "rows"       : self._row_count,
            "elapsed_s"  : elapsed,
            "avg_hz"     : round(self._row_count / elapsed, 2) if elapsed > 0 else 0,
            "file"       : str(self._current_file_path),
        }

    # ── Context manager support ────────────────
    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *_):
        self.stop()


# ──────────────────────────────────────────────
# Auto-logging loop (chạy ở background thread)
# ──────────────────────────────────────────────

class AutoLogger:
    """
    Chạy DataLogger ở background với tần suất cố định.
    Cần truyền vào các hàm callback đọc sensor.

    Ví dụ:
        def read_imu():   return IMUData(accel_x=0.1, gyro_z=0.05)
        def read_light(): return LightSensorData(lux_0=300)
        def read_state(): return ADCSState(yaw=0.2, wheel_rpm=1200)

        auto = AutoLogger(read_imu, read_light, read_state, rate_hz=10)
        auto.start()
        # ... làm việc khác ...
        auto.stop()
    """

    def __init__(self, imu_fn=None, light_fn=None, state_fn=None,
                 rate_hz: float = LOG_RATE_HZ, log_dir: Path = LOG_DIR):
        self._imu_fn   = imu_fn
        self._light_fn = light_fn
        self._state_fn = state_fn
        self._logger   = DataLogger(log_dir=log_dir, rate_hz=rate_hz)
        self._interval = 1.0 / rate_hz
        self._thread   : Optional[threading.Thread] = None
        self._stop_evt = threading.Event()

    def start(self) -> Path:
        path = self._logger.start()
        self._stop_evt.clear()
        self._thread = threading.Thread(target=self._loop, daemon=True, name="ADCSLogger")
        self._thread.start()
        return path

    def stop(self):
        self._stop_evt.set()
        if self._thread:
            self._thread.join(timeout=3.0)
        self._logger.stop()

    def _loop(self):
        while not self._stop_evt.is_set():
            t0 = time.monotonic()
            try:
                imu   = self._imu_fn()   if self._imu_fn   else None
                light = self._light_fn() if self._light_fn else None
                state = self._state_fn() if self._state_fn else None
                self._logger.log(imu, light, state)
            except Exception as exc:
                self._logger.log(error=str(exc))
            # Sleep chính xác
            elapsed = time.monotonic() - t0
            sleep_t = max(0.0, self._interval - elapsed)
            time.sleep(sleep_t)

    def get_stats(self) -> dict:
        return self._logger.get_stats()
