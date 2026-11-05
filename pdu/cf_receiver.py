#!/usr/bin/env python3
"""
cf_receiver.py - Nhận file CFDP (class 1) do cFS CF gửi xuống qua TO_LAB.

Luồng:  Pi: CF -> CF_CH0_TX_MID -> TO_LAB --UDP--> laptop (script này) -> file

Hai nguồn dữ liệu:
  --udp PORT   nghe trực tiếp UDP (mặc định 2234, cổng TO_LAB gửi xuống).
               Dùng khi cFS Ground System KHÔNG chạy (nó cũng bind cổng 2234).
  --gs         đăng ký vào luồng ZeroMQ của cFS Ground System
               (ipc:///tmp/GroundSystem-<user>), chạy song song với GUI.
               Cần: pip install pyzmq

Chỉ hỗ trợ CFDP class 1 (không cần gửi ACK ngược về).
Bố cục gói CF (cFE 7, không EDS): 6 byte CCSDS primary + 6 byte time
+ 4 byte spare = 16 byte header, sau đó là PDU CFDP.

Ví dụ:
  python3 cf_receiver.py --out received/
  python3 cf_receiver.py --gs --out received/
"""
import argparse
import getpass
import shlex
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

TLM_HDR_LEN = 16
DEFAULT_MIDS = (0x08C2, 0x08C3)  # CF_CH0_TX_MID, CF_CH1_TX_MID (TLM base 0x0800)
MAX_FILE = 256 * 1024 * 1024     # chặn file quá lớn / offset bậy

DIR_METADATA, DIR_EOF = 7, 4


class PduError(ValueError):
    pass


def parse_pdu(p: bytes) -> dict:
    """Giải mã header + nội dung PDU CFDP (CCSDS 727.0-B-5)."""
    if len(p) < 4:
        raise PduError("PDU quá ngắn")
    b0, b3 = p[0], p[3]
    is_data = bool((b0 >> 4) & 1)
    crc = bool((b0 >> 1) & 1)
    large = bool(b0 & 1)
    dlen = (p[1] << 8) | p[2]
    eid_len = ((b3 >> 4) & 7) + 1
    seg_meta = bool((b3 >> 3) & 1)
    seq_len = (b3 & 7) + 1
    hdr_len = 4 + 2 * eid_len + seq_len
    if len(p) < hdr_len + dlen:
        raise PduError(f"PDU bị cắt: có {len(p)}, cần {hdr_len + dlen}")
    src = int.from_bytes(p[4:4 + eid_len], "big")
    seq = int.from_bytes(p[4 + eid_len:4 + eid_len + seq_len], "big")
    body = p[hdr_len:hdr_len + dlen]
    if crc:
        body = body[:-2]
    out = {"key": (src, seq), "src": src, "seq": seq, "data": is_data}
    fss = 8 if large else 4
    if is_data:
        i = 0
        if seg_meta:
            i = 1 + (body[0] & 0x3F)
        off = int.from_bytes(body[i:i + fss], "big")
        out.update(kind="data", offset=off, payload=body[i + fss:])
        return out
    if not body:
        raise PduError("directive rỗng")
    code = body[0]
    if code == DIR_METADATA:
        i = 2
        size = int.from_bytes(body[i:i + fss], "big")
        i += fss
        n = body[i]
        srcname = body[i + 1:i + 1 + n].decode("utf-8", "replace")
        i += 1 + n
        n = body[i]
        dstname = body[i + 1:i + 1 + n].decode("utf-8", "replace")
        out.update(kind="metadata", size=size, src_name=srcname, dst_name=dstname)
    elif code == DIR_EOF:
        cond = body[1] >> 4
        cks = int.from_bytes(body[2:6], "big")
        size = int.from_bytes(body[6:6 + fss], "big")
        out.update(kind="eof", cond=cond, checksum=cks, size=size)
    else:
        out.update(kind=f"directive_{code}")
    return out


def modular_checksum(buf: bytes) -> int:
    """Checksum CFDP type 0: tổng các từ 32-bit big-endian."""
    buf = bytes(buf) + b"\0" * ((-len(buf)) % 4)
    s = 0
    for (w,) in struct.iter_unpack(">I", buf):
        s = (s + w) & 0xFFFFFFFF
    return s


def safe_name(name: str, fallback: str) -> str:
    """Chỉ lấy tên file, bỏ mọi đường dẫn (chống ghi ra ngoài thư mục --out)."""
    base = name.replace("\\", "/").rsplit("/", 1)[-1].strip()
    base = "".join(c for c in base if c.isalnum() or c in "._-")
    if not base or base.startswith("."):
        return fallback
    return base[:100]


class Transfer:
    def __init__(self, key):
        self.key = key
        self.name = None
        self.size = None
        self.eof_cks = None
        self.eof_time = None
        self.buf = bytearray()
        self.segs = []
        self.pdus = 0
        self.started = time.monotonic()

    def add(self, off, payload):
        end = off + len(payload)
        if end > MAX_FILE:
            return
        if len(self.buf) < end:
            self.buf.extend(b"\0" * (end - len(self.buf)))
        self.buf[off:end] = payload
        self.segs.append((off, end))

    def missing(self):
        """Các khoảng [a,b) chưa nhận được trong [0,size)."""
        if self.size is None:
            return None
        gaps, pos = [], 0
        for a, b in sorted(self.segs):
            if a > pos:
                gaps.append((pos, a))
            pos = max(pos, b)
        if pos < self.size:
            gaps.append((pos, self.size))
        return gaps

    def complete(self):
        return self.size is not None and self.eof_cks is not None and not self.missing()


class Receiver:
    def __init__(self, out_dir, grace, on_complete=None):
        self.out = Path(out_dir)
        self.out.mkdir(parents=True, exist_ok=True)
        self.grace = grace
        self.on_complete = on_complete
        self.tx = {}

    def unique_path(self, name):
        p = self.out / name
        n = 1
        while p.exists():
            p = self.out / f"{p.stem.rsplit('_dup', 1)[0]}_dup{n}{p.suffix}"
            n += 1
        return p

    def feed(self, pdu: bytes):
        try:
            m = parse_pdu(pdu)
        except (PduError, IndexError) as e:
            print(f"[bỏ qua] PDU lỗi: {e}")
            return
        t = self.tx.setdefault(m["key"], Transfer(m["key"]))
        t.pdus += 1
        k = m["kind"]
        if k == "metadata":
            t.size, t.name = m["size"], m["dst_name"]
            print(f"[{m['key']}] Metadata: '{m['dst_name']}' {m['size']} byte")
        elif k == "data":
            t.add(m["offset"], m["payload"])
        elif k == "eof":
            t.eof_cks, t.eof_time = m["checksum"], time.monotonic()
            if t.size is None:
                t.size = m["size"]
            print(f"[{m['key']}] EOF (cond={m['cond']}, size={m['size']})")
        self.poll()

    def poll(self):
        now = time.monotonic()
        for key, t in list(self.tx.items()):
            if t.complete():
                self.finish(t, partial=False)
            elif t.eof_time is not None and now - t.eof_time > self.grace:
                self.finish(t, partial=True)

    def finish(self, t, partial):
        fb = f"file_{t.key[0]}_{t.key[1]}.bin"
        name = safe_name(t.name or "", fb)
        gaps = t.missing()
        data = bytes(t.buf[:t.size]) if t.size is not None else bytes(t.buf)
        ok_cks = modular_checksum(data) == t.eof_cks if t.eof_cks is not None else False
        if partial or gaps or not ok_cks:
            name += ".partial"
        path = self.unique_path(name)
        path.write_bytes(data)
        status = "OK" if not (partial or gaps or not ok_cks) else "KHÔNG ĐẦY ĐỦ"
        print(f"[{t.key}] {status}: {path} ({len(data)} byte, {t.pdus} PDU, "
              f"checksum {'đúng' if ok_cks else 'SAI'})")
        if gaps:
            print(f"        thiếu: {gaps[:5]}{' ...' if len(gaps) > 5 else ''}")
        del self.tx[t.key]
        if self.on_complete and status == "OK":
            try:  # không dùng shell; tên file đã được làm sạch
                subprocess.Popen(shlex.split(self.on_complete) + [str(path)])
            except OSError as e:
                print(f"        --on-complete lỗi: {e}")


def cf_pdu_from_datagram(d: bytes, mids, hdr_len):
    if len(d) < hdr_len + 4:
        return None
    mid = struct.unpack(">H", d[:2])[0]
    if mids and mid not in mids:
        return None
    total = struct.unpack(">H", d[4:6])[0] + 7
    if total <= len(d):
        d = d[:total]
    return d[hdr_len:]


def run_udp(port, rx, mids, hdr_len):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024)
    try:
        s.bind(("", port))
    except OSError as e:
        sys.exit(f"Không bind được UDP {port}: {e}\n"
                 f"Nếu cFS Ground System đang chạy (nó giữ cổng {port}), dùng --gs.")
    s.settimeout(0.5)
    print(f"Đang nghe UDP :{port} (MID {[hex(m) for m in mids] or 'tất cả'}) ...")
    while True:
        try:
            d, _ = s.recvfrom(65535)
        except socket.timeout:
            rx.poll()
            continue
        pdu = cf_pdu_from_datagram(d, mids, hdr_len)
        if pdu is not None:
            rx.feed(pdu)


def run_gs(rx, mids, hdr_len):
    try:
        import zmq
    except ImportError:
        sys.exit("Thiếu pyzmq: pip install pyzmq")
    ctx = zmq.Context()
    sub = ctx.socket(zmq.SUB)
    addr = f"ipc:///tmp/GroundSystem-{getpass.getuser()}"
    sub.connect(addr)
    sub.setsockopt(zmq.SUBSCRIBE, b"GroundSystem.")
    sub.RCVTIMEO = 500
    print(f"Đang nghe Ground System ({addr}) (MID {[hex(m) for m in mids] or 'tất cả'}) ...")
    while True:
        try:
            _topic, d = sub.recv_multipart()
        except zmq.Again:
            rx.poll()
            continue
        pdu = cf_pdu_from_datagram(d, mids, hdr_len)
        if pdu is not None:
            rx.feed(pdu)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default="received", help="thư mục lưu file nhận được")
    ap.add_argument("--udp", type=int, default=2234, help="cổng UDP (mặc định 2234)")
    ap.add_argument("--gs", action="store_true", help="nghe qua Ground System (ZeroMQ)")
    ap.add_argument("--mids", default="0x08C2,0x08C3",
                    help="MID của CF TX (phân tách bằng dấu phẩy), 'any' = nhận hết")
    ap.add_argument("--hdr-len", type=int, default=TLM_HDR_LEN,
                    help="độ dài header cFE tlm (mặc định 16)")
    ap.add_argument("--grace", type=float, default=2.0,
                    help="giây chờ gói còn thiếu sau EOF")
    ap.add_argument("--on-complete", default=None, metavar="CMD",
                    help="lệnh chạy sau khi nhận xong 1 file đúng; đường dẫn file được "
                         "thêm vào cuối. Vd: --on-complete 'python3 camera/process.py'")
    a = ap.parse_args()
    sys.stdout.reconfigure(line_buffering=True)
    mids = () if a.mids.lower() == "any" else tuple(int(x, 0) for x in a.mids.split(","))
    rx = Receiver(a.out, a.grace, a.on_complete)
    try:
        run_gs(rx, mids, a.hdr_len) if a.gs else run_udp(a.udp, rx, mids, a.hdr_len)
    except KeyboardInterrupt:
        print("\nDừng.")


if __name__ == "__main__":
    main()
