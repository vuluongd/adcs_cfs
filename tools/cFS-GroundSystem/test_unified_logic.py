"""Run: python3 -m unittest -v test_unified_logic.py"""
import math
from pathlib import Path
import socket
import struct
import unittest

from unified_logic import (
    APP_COMMANDS, APP_IDS, build_command_argv, decode_fields, get_mid,
    noise_std, parse_pages, send_command, validate_ip,
)

HERE = Path(__file__).resolve().parent
NASA_GROUND = HERE
TLM_DIR = NASA_GROUND / 'Subsystems' / 'tlmGUI'

class ProtocolTests(unittest.TestCase):
    def test_addresses(self):
        self.assertEqual(validate_ip('127.0.0.1'), '127.0.0.1')
        self.assertEqual(validate_ip('192.168.1.50'), '192.168.1.50')
        for bad in ('', 'hello', '8.8.8.8', '0.0.0.0', '224.0.0.1'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                validate_ip(bad)

    def test_catalog_mid(self):
        self.assertEqual(APP_IDS['IMU_APP'], 0x18D0)
        self.assertEqual(APP_IDS['ACT_APP'], 0x18E0)
        self.assertIn('STOP_WHEEL', APP_COMMANDS['ACT_APP'])

    def test_command_array_noop(self):
        argv = build_command_argv(Path('/tmp/cmdUtil'), '127.0.0.1', 1234,
                                  'IMU_APP', 'NOOP')
        self.assertIn('--pktid=0x18D0', argv)
        self.assertIn('--cmdcode=0', argv)
        self.assertFalse(any(x.startswith('--float') for x in argv))

    def test_command_speed_limits(self):
        argv = build_command_argv(Path('/tmp/cmdUtil'), '127.0.0.1', 1234,
                                  'ACT_APP', 'SET_WHEEL_SPEED', 20.5)
        self.assertIn('--float=20.5', argv)
        self.assertIn('--cmdcode=4', argv)
        for bad in (61, -61, math.inf, math.nan):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                build_command_argv(Path('/tmp/cmdUtil'), '127.0.0.1', 1234,
                                   'ACT_APP', 'SET_WHEEL_SPEED', bad)

    def test_tlm_enable(self):
        argv = build_command_argv(Path('/tmp/cmdUtil'), '127.0.0.1', 1234,
                                  'TO_LAB', 'ENABLE_TLM', tlm_destination='192.168.1.20')
        self.assertIn('--string=17:192.168.1.20', argv)
        self.assertIn('--cmdcode=6', argv)

    def test_incompatible_command(self):
        with self.assertRaises(ValueError):
            build_command_argv(Path('/tmp/cmdUtil'), '127.0.0.1', 1234,
                               'CFE_ES', 'SET_WHEEL_SPEED')

    def test_telemetry_pages(self):
        pages = parse_pages(TLM_DIR / 'telemetry-pages.txt')
        self.assertIn(0x08D4, {p.mid for p in pages})
        self.assertIn(0x08E4, {p.mid for p in pages})
        self.assertIn(0x08DC, {p.mid for p in pages})

    def test_packet_decode_imu(self):
        pkt = struct.pack('>HHH', 0x08D4, 0xC000, 0) + bytes(6)
        pkt += struct.pack('<ffffffI4B', 0.16, 0.12, 0.5, 0.01, 23.5, .1,
                           15, 1, 0, 1, 0)
        values = decode_fields(pkt, TLM_DIR / 'imu-app-data-tlm.txt')
        self.assertEqual(get_mid(pkt), 0x08D4)
        self.assertAlmostEqual(values['Angle (rad)'], 0.5)
        self.assertAlmostEqual(values['Rate (rad/s)'], .12)
        self.assertEqual(values['Valid'], 1)
        self.assertEqual(values['Simulated'], 0)

    def test_packet_decode_wheel(self):
        pkt = struct.pack('>HHH', 0x08E4, 0xC000, 0) + bytes(6)
        pkt += struct.pack('<ffffI6BbB', 10, 10, 9.8, 20, 11, 1, 0, 0, 0, 1, 0, 1, 1)
        v = decode_fields(pkt, TLM_DIR / 'act-app-wheel-data-tlm.txt')
        self.assertAlmostEqual(v['Meas Omega (rad/s)'], 9.8, places=4)
        self.assertEqual(v['Measured'], 1)

    def test_noise(self):
        self.assertIsNone(noise_std([0.0] * 7))
        self.assertEqual(noise_std([0.5] * 10), 0.0)

if __name__ == '__main__':
    unittest.main()
