#!/usr/bin/env python3
"""
cfs_cmd.py - Gửi lệnh tới cFS qua CI_LAB (UDP 1234), không cần Ground System GUI.

MID tính từ cấu hình repo adcs_cfs (cpu1: CMD base 0x1800):
  TO_LAB  0x1800|0x80 = 0x1880   (CC 6  = OUTPUT_ENABLE, payload: dest_IP[16])
  CF      0x1800|0xB3 = 0x18B3   (CC 0  = NOOP, CC 2 = TX_FILE (compat), CC 32 = TX_FILE)

Ví dụ (Pi có IP 192.168.1.50, laptop 192.168.1.20):
  python3 cfs_cmd.py --host 192.168.1.50 to-enable --ip 192.168.1.20
  python3 cfs_cmd.py --host 192.168.1.50 cf-noop
  python3 cfs_cmd.py --host 192.168.1.50 cf-tx --src /cf/fake_001.png --dst fake_001.png

Lưu ý: payload của CF theo thứ tự byte của máy chạy cFS (Pi/x86 = little-endian).
"""
import argparse
import socket
import struct

CMD_BASE = 0x1800
TOPIC_TO_LAB_CMD = 0x80
TOPIC_CF_CMD = 0xB3
CF_FILENAME_MAX_LEN = 64


def build_cmd(mid: int, cc: int, payload: bytes = b"", seq: int = 0) -> bytes:
    total = 6 + 2 + len(payload)
    pkt = bytearray(struct.pack(">HHH", mid, 0xC000 | (seq & 0x3FFF), total - 7))
    pkt += bytes([cc & 0x7F, 0]) + payload
    chk = 0xFF
    for b in pkt:
        chk ^= b
    pkt[7] = chk
    return bytes(pkt)


def fixed_str(s: str, n: int) -> bytes:
    b = s.encode()
    if len(b) >= n:
        raise SystemExit(f"Chuỗi quá dài (tối đa {n - 1}): {s}")
    return b.ljust(n, b"\0")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", required=True, help="IP của máy chạy cFS (Pi)")
    ap.add_argument("--port", type=int, default=1234, help="cổng CI_LAB")
    ap.add_argument("--cmd-base", type=lambda x: int(x, 0), default=CMD_BASE)
    ap.add_argument("--endian", choices=["little", "big"], default="little",
                    help="byte order của máy chạy cFS (payload)")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("to-enable", help="bật TO_LAB gửi telemetry tới laptop")
    p.add_argument("--ip", required=True, help="IP của laptop")

    sub.add_parser("cf-noop", help="NOOP của CF (kiểm tra kết nối)")

    p = sub.add_parser("cf-tx", help="CF gửi 1 file")
    p.add_argument("--src", required=True, help="đường dẫn trên Pi, vd /cf/fake_001.png")
    p.add_argument("--dst", required=True, help="tên file đích")
    p.add_argument("--dest-id", type=int, default=23, help="entity ID đích")
    p.add_argument("--chan", type=int, default=0)
    p.add_argument("--priority", type=int, default=0)
    p.add_argument("--cfdp-class", type=int, choices=[1, 2], default=1)
    p.add_argument("--delete", action="store_true", help="xoá file nguồn sau khi gửi")
    p.add_argument("--cc", type=int, default=2, choices=[2, 32],
                   help="function code: 2 = TX_FILE_COMPAT, 32 = TX_FILE")

    a = ap.parse_args()
    e = "<" if a.endian == "little" else ">"

    if a.cmd == "to-enable":
        pkt = build_cmd(a.cmd_base | TOPIC_TO_LAB_CMD, 6, fixed_str(a.ip, 16))
    elif a.cmd == "cf-noop":
        pkt = build_cmd(a.cmd_base | TOPIC_CF_CMD, 0)
    else:
        payload = struct.pack(e + "BBBBI", a.cfdp_class - 1, 0 if a.delete else 1,
                              a.chan, a.priority, a.dest_id)
        payload += fixed_str(a.src, CF_FILENAME_MAX_LEN)
        payload += fixed_str(a.dst, CF_FILENAME_MAX_LEN)
        pkt = build_cmd(a.cmd_base | TOPIC_CF_CMD, a.cc, payload)

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.sendto(pkt, (a.host, a.port))
    print(f"Đã gửi {a.cmd} ({len(pkt)} byte) tới {a.host}:{a.port}: {pkt[:10].hex()}...")


if __name__ == "__main__":
    main()
