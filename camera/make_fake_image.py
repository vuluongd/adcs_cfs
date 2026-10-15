#!/usr/bin/env python3
"""
make_fake_image.py - Tao anh PNG gia (thay cho anh camera_app chup) tren OBC.

Chi dung thu vien chuan cua Python. Anh la dai mau + o ban co + khung, du phuc tap
de nen khong qua nho, nen se can nhieu PDU khi truyen.

Vi du:
    python3 make_fake_image.py                         # tao /tmp/camera/fake_001.png
    python3 make_fake_image.py --out /cf/fake_001.png --width 320 --height 240
"""

import argparse
import hashlib
import os
import random
import struct
import zlib


def make_png(width, height, seed):
    rnd = random.Random(seed)
    rows = []
    for y in range(height):
        row = bytearray([0])                       # filter 0
        for x in range(width):
            r = int(255 * x / max(1, width - 1))
            g = int(255 * y / max(1, height - 1))
            b = 255 - r
            if ((x // 20) + (y // 20)) % 2 == 0:   # o ban co
                r, g, b = 255 - r, 255 - g, 255 - b
            n = rnd.randint(-12, 12)               # nhieu nhe de khong nen qua manh
            row += bytes((max(0, min(255, r + n)), max(0, min(255, g + n)), max(0, min(255, b + n))))
        rows.append(bytes(row))
    raw = b"".join(rows)

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 6))
            + chunk(b"IEND", b""))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/tmp/camera/fake_001.png")
    ap.add_argument("--width", type=int, default=320)
    ap.add_argument("--height", type=int, default=240)
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()

    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    data = make_png(a.width, a.height, a.seed)
    with open(a.out, "wb") as f:
        f.write(data)
    print(f"Da tao {a.out}")
    print(f"  kich thuoc: {len(data)} byte ({a.width}x{a.height})")
    print(f"  sha256    : {hashlib.sha256(data).hexdigest()}")


if __name__ == "__main__":
    main()
