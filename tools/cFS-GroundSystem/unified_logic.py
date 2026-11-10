"""Protocol helper for the unified cFS GroundSystem operator UI.

Uses NASA's existing cmdUtil executable for CCSDS command construction.
No Qt dependency: validation and telemetry decoding are independently testable.

Based on the application IDs, command function codes and telemetry definition
files from the adcs_cfs source snapshot supplied by the project author.
"""
from __future__ import annotations

import csv
from dataclasses import dataclass
import ipaddress
import math
from pathlib import Path
import statistics
import struct
import subprocess
from typing import Optional

APP_IDS = {
    'CFE_ES': 0x1806,
    'CFE_SB': 0x1803,
    'CFE_TBL': 0x1804,
    'CFE_TIME': 0x1805,
    'CFE_EVS': 0x1801,
    'CI_LAB': 0x1884,
    'TO_LAB': 0x1880,
    'IMU_APP': 0x18D0,
    'LIGHT_APP': 0x18D8,
    'ACT_APP': 0x18E0,
    'ADCS_APP': 0x18E8,
    'CAMERA_APP': 0x1882,
}

# A small, verified set of no-parameter commands. No arbitrary code injection.
APP_COMMANDS = {
    'CFE_ES': ['NOOP'], 'CFE_SB': ['NOOP'], 'CFE_TBL': ['NOOP'],
    'CFE_TIME': ['NOOP'], 'CFE_EVS': ['NOOP'], 'CI_LAB': ['NOOP'],
    'TO_LAB': ['NOOP', 'ENABLE_TLM'],
    'IMU_APP': ['NOOP', 'RESET_COUNTERS', 'RECALIBRATE', 'RESET_ANGLE'],
    'LIGHT_APP': ['NOOP', 'RESET_COUNTERS'],
    'ACT_APP': ['NOOP', 'RESET_COUNTERS', 'SET_WHEEL_SPEED', 'STOP_WHEEL'],
    'ADCS_APP': ['NOOP', 'RESET_COUNTERS'],
    'CAMERA_APP': ['NOOP', 'RESET_COUNTERS'],
}

CODE_BY_COMMAND = {
    'NOOP': 0, 'RESET_COUNTERS': 1, 'RECALIBRATE': 4,
    'RESET_ANGLE': 5, 'SET_WHEEL_SPEED': 4, 'STOP_WHEEL': 5,
    'ENABLE_TLM': 6,
}
SENSITIVE = {'RECALIBRATE', 'RESET_ANGLE', 'SET_WHEEL_SPEED'}

@dataclass(frozen=True)
class TelemetryPage:
    name: str
    mid: int
    definition: str


def validate_ip(ip: str) -> str:
    """Restrict command destinations to local/private IPv4 addresses."""
    try:
        addr = ipaddress.IPv4Address(ip.strip())
    except ipaddress.AddressValueError as exc:
        raise ValueError('IP không hợp lệ. Ví dụ: 127.0.0.1 hoặc 192.168.1.50') from exc
    if not (addr.is_private or addr.is_loopback or addr.is_link_local) or addr.is_multicast or addr.is_unspecified:
        raise ValueError('Chỉ cho phép gửi lệnh tới IPv4 nội bộ / loopback')
    return str(addr)


def parse_pages(path: Path) -> list[TelemetryPage]:
    pages = []
    with open(path, newline='', encoding='utf-8') as stream:
        for row in csv.reader(stream, skipinitialspace=True):
            if not row or not row[0].strip() or row[0].lstrip().startswith('#'):
                continue
            if len(row) < 4:
                continue
            try:
                pages.append(TelemetryPage(row[0].strip(), int(row[2].strip(), 16), row[3].strip()))
            except ValueError:
                continue
    return pages


def get_mid(packet: bytes) -> Optional[int]:
    return int.from_bytes(packet[:2], 'big') if len(packet) >= 6 else None


def decode_fields(packet: bytes, definition: Path, offset: int = 0,
                  endian: str = '<') -> dict[str, object]:
    """Decode NASA CSV definitions, without assuming receipt of an ACK.

    A definition's offset is applied to the entire CCSDS datagram. For this
    project's IMU/WHEEL/LIGHT definitions, fields start at byte 12. GroundSystem
    legacy versions may require +4: choose via the UI's header-offset control.
    """
    decoded = {}
    if not definition.is_file():
        return decoded
    if offset < 0 or offset > 32:
        raise ValueError('Telemetry header offset ngoài phạm vi')
    with open(definition, newline='', encoding='utf-8') as stream:
        for row in csv.reader(stream, skipinitialspace=True):
            if not row or row[0].lstrip().startswith('#') or len(row) < 5:
                continue
            try:
                name, position, size, fmt = row[0].strip(), int(row[1]), int(row[2]), row[3].strip()
                if size <= 0 or size > 256:
                    continue
                if fmt in ('s', 'p'):
                    fmt = str(size) + fmt
                code = endian + fmt.lstrip('<>!=@')
                if struct.calcsize(code) != size:
                    continue
                start = position + offset
                if start < 0 or start + size > len(packet):
                    continue
                value = struct.unpack_from(code, packet, start)[0]
                if isinstance(value, bytes):
                    value = value.decode('utf-8', errors='replace').rstrip('\x00')
                decoded[name] = value
            except (ValueError, struct.error, IndexError):
                continue
    return decoded


def noise_std(values: list[float]) -> Optional[float]:
    finite = [v for v in values if math.isfinite(v)]
    if len(finite) < 8:
        return None
    return statistics.stdev(finite)


def build_command_argv(cmdutil: Path, ip: str, port: int, app: str,
                       name: str, speed: float = 0.0,
                       tlm_destination: str = '127.0.0.1',
                       max_wheel: float = 60.0) -> list[str]:
    if app not in APP_COMMANDS or name not in APP_COMMANDS[app]:
        raise ValueError('Lệnh chưa được xác minh cho ứng dụng này')
    ip = validate_ip(ip)
    if not 1 <= int(port) <= 65535:
        raise ValueError('UDP command port không hợp lệ')
    mid = APP_IDS[app]
    argv = [str(cmdutil), '--host=' + ip, '--port=' + str(int(port)),
            '--protocol=cfsv1', '--pktid=0x%04X' % mid,
            '--cmdcode=' + str(CODE_BY_COMMAND[name]), '--endian=LE']
    if name == 'SET_WHEEL_SPEED':
        if not math.isfinite(speed) or abs(speed) > max_wheel:
            raise ValueError('Tốc độ ngoài giới hạn thử nghiệm ±%g rad/s' % max_wheel)
        argv.append('--float=' + format(float(speed), '.8g'))
    if name == 'ENABLE_TLM':
        destination = validate_ip(tlm_destination)
        argv.append('--string=17:' + destination)
    return argv


def send_command(argv: list[str], timeout: float = 3.0) -> str:
    cmdutil = Path(argv[0])
    if not cmdutil.is_file():
        raise RuntimeError('cmdUtil chưa được biên dịch. Chạy: make -C Subsystems/cmdUtil')
    try:
        result = subprocess.run(argv, timeout=timeout, capture_output=True, text=True, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise RuntimeError('Lỗi gọi cmdUtil: ' + str(exc)) from exc
    if result.returncode != 0:
        raise RuntimeError('cmdUtil thất bại: ' + (result.stderr or result.stdout)[-350:])
    return 'SENT_UNCONFIRMED: Đã gửi UDP, chưa phải xác nhận thực thi từ cFS.'
