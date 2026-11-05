#!/usr/bin/env python3
"""
fake_cf_sender.py - Giả lập CF + TO_LAB để thử receiver mà KHÔNG cần chạy cFS.

Đọc 1 file ảnh, cắt thành PDU CFDP class 1 (Metadata, File Data, EOF), bọc
trong gói telemetry cFE giống hệt CF_CH0_TX_MID rồi gửi UDP tới receiver.

Ví dụ:
  terminal 1:  python3 cf_receiver.py --out received/
  terminal 2:  python3 fake_cf_sender.py camera/fake_001.png
  thử mất gói: python3 fake_cf_sender.py camera/fake_001.png --drop-every 7
"""
import argparse
import socket
import struct
import time
from pathlib import Path

CH0_TX_MID = 0x08C2
EID_LEN = SEQ_LEN = 4


def modular_checksum(buf: bytes) -> int:
    buf = buf + b"\0" * ((-len(buf)) % 4)
    return sum(w for (w,) in struct.iter_unpack(">I", buf)) & 0xFFFFFFFF


def pdu_header(is_data, data_len, src, seq, dst):
    b0 = (1 << 5) | (int(is_data) << 4) | (1 << 2)   # version 1, class 1 (unack)
    b3 = ((EID_LEN - 1) << 4) | (SEQ_LEN - 1)
    return (struct.pack(">BHB", b0, data_len, b3) + src.to_bytes(EID_LEN, "big")
            + seq.to_bytes(SEQ_LEN, "big") + dst.to_bytes(EID_LEN, "big"))


def lv(s: bytes) -> bytes:
    return bytes([len(s)]) + s


def tlm_wrap(pdu: bytes, mid: int, seq: int) -> bytes:
    total = 16 + len(pdu)
    hdr = struct.pack(">HHH", mid, 0xC000 | (seq & 0x3FFF), total - 7)
    return hdr + b"\0" * 6 + b"\0" * 4 + pdu


def build_pdus(data: bytes, dst_name: str, src_eid, seq, dst_eid, chunk):
    pdus = []
    body = bytes([7, 0]) + struct.pack(">I", len(data)) + lv(dst_name.encode()) + lv(dst_name.encode())
    pdus.append(pdu_header(False, len(body), src_eid, seq, dst_eid) + body)
    for off in range(0, len(data), chunk):
        body = struct.pack(">I", off) + data[off:off + chunk]
        pdus.append(pdu_header(True, len(body), src_eid, seq, dst_eid) + body)
    body = bytes([4, 0]) + struct.pack(">II", modular_checksum(data), len(data))
    pdus.append(pdu_header(False, len(body), src_eid, seq, dst_eid) + body)
    return pdus


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=2234)
    ap.add_argument("--chunk", type=int, default=480)
    ap.add_argument("--src-id", type=int, default=25)
    ap.add_argument("--dest-id", type=int, default=23)
    ap.add_argument("--seq", type=int, default=1)
    ap.add_argument("--rate", type=float, default=200.0, help="gói/giây")
    ap.add_argument("--drop-every", type=int, default=0, help="bỏ mỗi gói thứ N (thử mất gói)")
    a = ap.parse_args()

    data = Path(a.file).read_bytes()
    pdus = build_pdus(data, Path(a.file).name, a.src_id, a.seq, a.dest_id, a.chunk)
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sent = 0
    for i, p in enumerate(pdus):
        if a.drop_every and i and i % a.drop_every == 0 and i != len(pdus) - 1:
            continue
        s.sendto(tlm_wrap(p, CH0_TX_MID, i), (a.host, a.port))
        sent += 1
        time.sleep(1.0 / a.rate)
    print(f"Đã gửi {sent}/{len(pdus)} gói ({len(data)} byte ảnh) tới {a.host}:{a.port}")


if __name__ == "__main__":
    main()
