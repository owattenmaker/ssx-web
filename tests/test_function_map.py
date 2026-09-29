import csv
import json
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from function_map import write_function_map


class FunctionMapTests(unittest.TestCase):
    def test_split_and_preserve_stub_entries(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'code.cpp').write_text('// Address: 0x100000 - 0x100080\n')
            (root / 'stub.cpp').write_text('void sub_00100080_0x100080(uint8_t*) {}\n')
            (root / 'entries.json').write_text(json.dumps([{'address': '0x100040', 'end': '0x100080'}]))
            count = write_function_map(root, root / 'entries.json', root / 'map.csv')
            with (root / 'map.csv').open() as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(count, 3)
            self.assertEqual([row['Start'] for row in rows], ['0x100000', '0x100040', '0x100080'])
            self.assertEqual(rows[0]['End'], '0x100040')
