import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from inspect_disc import Disc, inspect_big, inspect_elf


def record(name, extent, size, directory=False):
    result = bytearray(33 + len(name) + (len(name) % 2 == 0))
    result[0] = len(result)
    result[2:6] = extent.to_bytes(4, 'little')
    result[10:14] = size.to_bytes(4, 'little')
    result[25] = 2 if directory else 0
    result[32] = len(name)
    result[33:33 + len(name)] = name
    return result


class DiscTests(unittest.TestCase):
    def make_disc(self, file_record=None):
        data = bytearray(24 * 2048)
        pvd = memoryview(data)[16 * 2048:17 * 2048]
        pvd[:7] = b'\x01CD001\x01'
        pvd[40:72] = b'TEST'.ljust(32)
        pvd[128:130] = (2048).to_bytes(2, 'little')
        root = record(b'\0', 20, 2048, True)
        pvd[156:156 + len(root)] = root
        contents = root + record(b'\1', 20, 2048, True)
        contents += file_record if file_record is not None else record(b'HELLO.TXT;1', 21, 5)
        data[20 * 2048:20 * 2048 + len(contents)] = contents
        data[21 * 2048:21 * 2048 + 5] = b'hello'
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        path = Path(temp.name) / 'test.iso'
        path.write_bytes(data)
        return path

    def test_extent_read_and_version_removal(self):
        disc = Disc(self.make_disc())
        self.addCleanup(disc.close)
        self.assertEqual(disc.volume, 'TEST')
        self.assertEqual(disc.file('hello.txt'), b'hello')

    def test_reject_out_of_bounds_extent(self):
        with self.assertRaisesRegex(ValueError, 'outside image'):
            Disc(self.make_disc(record(b'BAD;1', 99, 8)))

    def test_reject_directory_cycle(self):
        with self.assertRaisesRegex(ValueError, 'Cyclic'):
            Disc(self.make_disc(record(b'LOOP', 20, 2048, True)))

    def test_reject_path_traversal(self):
        with self.assertRaisesRegex(ValueError, 'Unsafe'):
            Disc(self.make_disc(record(b'../BAD', 21, 5)))

    def test_read_bounds(self):
        disc = Disc(self.make_disc())
        self.addCleanup(disc.close)
        with self.assertRaises(ValueError):
            disc.read(-1, 1)
        with self.assertRaises(ValueError):
            disc.read(disc.size, 1)

    def test_big_empty_wildcard_and_member(self):
        directory = struct.pack('>II', 0, 0) + b'*.bnk\0'
        end = 16 + len(directory) + 8 + 6
        directory += struct.pack('>II', end, 4) + b'x.bin\0'
        data = b'BIGF' + struct.pack('<I', end + 4) + struct.pack('>II', 2, end) + directory + b'data'

        class Reader:
            def read(self, offset, size):
                return data[offset:offset + size]

        result = inspect_big(Reader(), dict(offset=0, size=len(data), path='test.big'))
        self.assertEqual(result['files'][0]['size'], 0)
        self.assertEqual(result['files'][1]['disc_offset'], end)
        self.assertEqual(result['files'][1]['path'], 'x.bin')
        with self.assertRaises(ValueError):
            inspect_big(Reader(), dict(offset=0, size=len(data) - 1, path='test.big'))

    def test_reject_wrong_executable(self):
        with self.assertRaises(ValueError):
            inspect_elf(b'not an ELF')


if __name__ == '__main__':
    unittest.main()
