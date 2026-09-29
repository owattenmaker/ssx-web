import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from compact_registration import compact


class RegistrationTests(unittest.TestCase):
    def test_adjacent_assignments_with_gap_and_overwrite(self):
        source = '''void init() {
        g_ps2RecompiledFunctionTable[10] = first; // 0x100028
        g_ps2RecompiledFunctionTable[11] = first; // 0x10002c
        g_ps2RecompiledFunctionTable[13] = second; // 0x100034
        g_ps2RecompiledFunctionTable[11] = second; // 0x10002c
}
'''
        result = compact(source)
        self.assertIn('slot = 10; slot <= 11;', result)
        self.assertNotIn('Table[12]', result)
        self.assertGreater(result.index('Table[11] = second'), result.index('slot = 10'))

    def test_reject_unrecognized_format(self):
        with self.assertRaises(ValueError):
            compact('unexpected generator output')
