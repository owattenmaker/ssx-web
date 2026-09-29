import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from reference_replay import pad_frame,build


class ReferenceReplayTests(unittest.TestCase):
    def test_neutral_and_four_shoulders(self):
        neutral=pad_frame()
        self.assertEqual(neutral[:6],bytes((255,255,127,127,127,127)))
        self.assertEqual(len(neutral),18)
        for mask in range(16):
            names=[name for i,name in enumerate(('L1','L2','R1','R2')) if mask&(1<<i)]
            data=pad_frame(names)
            self.assertEqual([bool(data[i]) for i in (14,16,15,17)],[bool(mask&(1<<i)) for i in range(4)])

    def test_press_edges_and_deterministic_serialization(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);baseline=p/'baseline.p2s';baseline.write_bytes(b'test fixture')
            spec=dict(baseline=str(baseline),frames=10,events=[dict(start=2,end=5,buttons=['Cross'])])
            a,b=p/'a.p2m2',p/'b.p2m2'
            build(spec,a);build(spec,b)
            self.assertEqual(a.read_bytes(),b.read_bytes())
            data=a.read_bytes();self.assertEqual(len(data),570+10*36)
            self.assertEqual(struct.unpack_from('<II?',data,561),(10,0,True))
            for frame in range(10):self.assertEqual(data[570+frame*36+12],255 if 2<=frame<5 else 0)
            self.assertEqual(Path(str(a)+'_SaveState.p2s').read_bytes(),b'test fixture')

    def test_invalid_controller_input(self):
        for kwargs in ({'left':(0,)},{'right':(0,300)},{'buttons':['Missing']}):
            with self.assertRaises(ValueError):pad_frame(**kwargs)


if __name__=='__main__':unittest.main()
