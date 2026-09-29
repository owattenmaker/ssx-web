"""Sky exporter helpers on synthetic data; no owned assets required."""
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from import_sky import sky_location_for, painter_sections, painted_entries, fog_entries, write_png


def painter_record(sections):
    """Build a kind-15 record with the given {type: payload-list} sections."""
    offsets = [0xffffffff] * 13
    body = bytearray()
    base = 0x40
    for kind, payloads in sorted(sections.items()):
        offsets[kind - 1] = base + len(body)
        count = len(payloads)
        header = 0x14 + 8 * (count - 1)
        section = bytearray(struct.pack('<3I', header, count, 0xc))
        payload_at = header + 0x30
        for i in range(count):
            section += struct.pack('<2I', kind, payload_at + 28 * i)
        section += bytes(0x30)
        for values in payloads:
            section += struct.pack('<7f', *values)
        body += section
    return struct.pack('<3I', 0x10, 0xe, 0x40) + struct.pack('<13I', *offsets) + bytes(body)


class SkyTests(unittest.TestCase):
    def test_sky_location_follows_peak_letter(self):
        self.assertEqual(sky_location_for('ARA1'), 'ASKY')
        self.assertEqual(sky_location_for('A'), 'ASKY')
        self.assertEqual(sky_location_for('ERA5'), 'ESKY')
        with self.assertRaises(ValueError):
            sky_location_for('TRANSP')

    def test_painted_fog_entries(self):
        fog = [(-0.1, 2.0, 3000.0, 10000.0, 0.7, 0.82, 1.0), (-0.03, 3.0, 3000.0, 20000.0, 0.9, 0.93, 1.0)]
        sun = [(-0.1, 10.57, 124.9, 0.9, 0.9, 0.9, 0.9)]
        sections = painter_sections(painter_record({5: fog, 9: sun}))
        self.assertEqual(sorted(sections), [5, 9])
        entries = fog_entries(sections[5])
        self.assertEqual(len(entries), 2)
        self.assertEqual(entries[0]['near_cm'], 3000.0)
        self.assertEqual(entries[1]['far_cm'], 20000.0)
        self.assertAlmostEqual(entries[0]['color'][1], 0.82, places=5)
        self.assertEqual(painted_entries(sections[9], 3)[0][0], 9)
        with self.assertRaises(ValueError):
            fog_entries(sections[9])

    def test_empty_painter_record(self):
        self.assertEqual(painter_sections(bytes(8)), {})
        with self.assertRaises(ValueError):
            painter_sections(bytes(0x40))

    def test_png_roundtrip_header(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'x.png'
            write_png(path, 2, 1, bytes([255, 0, 0, 255, 0, 255, 0, 128]))
            data = path.read_bytes()
            self.assertTrue(data.startswith(b'\x89PNG'))
            self.assertEqual(struct.unpack_from('>II', data, 16), (2, 1))
            idat = data.index(b'IDAT') + 4
            size = struct.unpack_from('>I', data, idat - 8)[0]
            self.assertEqual(zlib.decompress(data[idat:idat + size]), b'\0\xff\0\0\xff\0\xff\0\x80')
            with self.assertRaises(ValueError):
                write_png(path, 2, 2, bytes(4))


if __name__ == '__main__':
    unittest.main()
