"""
Đọc và phân tích file CSV log — dùng để kiểm tra dữ liệu sau khi thu thập.
Chạy: python read_log.py logs/adcs_log_YYYYMMDD_HHMMSS.csv
"""

import sys
import csv
from pathlib import Path
from datetime import datetime


def read_log(filepath: str):
    path = Path(filepath)
    if not path.exists():
        print(f"❌ Không tìm thấy file: {filepath}")
        return

    rows = []
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)

    if not rows:
        print("⚠ File rỗng.")
        return

    # ── Thống kê cơ bản ──────────────────────
    n = len(rows)
    duration = float(rows[-1]["elapsed_s"]) - float(rows[0]["elapsed_s"])
    avg_hz   = round(n / duration, 2) if duration > 0 else 0

    print(f"\n📁 File    : {path.name}")
    print(f"📊 Số dòng : {n}")
    print(f"⏱ Thời gian: {duration:.2f}s")
    print(f"📡 Tần suất: {avg_hz} Hz")

    # ── Đếm lỗi ──────────────────────────────
    errors = [r for r in rows if r.get("error_msg")]
    imu_missing   = [r for r in rows if r.get("imu_ok") == "False"]
    light_missing = [r for r in rows if r.get("light_ok") == "False"]

    print(f"\n🔴 Lỗi ghi nhận: {len(errors)}")
    print(f"   IMU thiếu dữ liệu : {len(imu_missing)} dòng")
    print(f"   Light thiếu dữ liệu: {len(light_missing)} dòng")

    # ── In 5 dòng đầu ────────────────────────
    print("\n🔎 5 dòng đầu:")
    print(f"  {'#':<4} {'time(s)':<10} {'yaw(rad)':<12} {'rpm':<10} {'mode'}")
    print("  " + "-"*50)
    for i, r in enumerate(rows[:5]):
        print(f"  {i+1:<4} {r['elapsed_s']:<10} {r['yaw']:<12} {r['wheel_rpm']:<10} {r['control_mode']}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        # Tự động lấy file log mới nhất trong thư mục ./logs/
        logs = sorted(Path("./logs").glob("adcs_log_*.csv"))
        if not logs:
            print("Dùng: python read_log.py <đường_dẫn_file.csv>")
            sys.exit(1)
        read_log(str(logs[-1]))
    else:
        read_log(sys.argv[1])
