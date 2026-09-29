import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from animation_curves import CurvePacket
from animation_bank import game_hash


class AnimationCurveTests(unittest.TestCase):
    def fixture(self):
        data=bytearray((9,0x01,0x23,0x45,0x67,0x80))
        def floats(values):
            for value in values:data.extend(struct.pack('<f',value)[1:])
        floats([2]);floats([3,4]);floats([2,3,4]);floats([1,2,3,4])
        floats([1,5,-1]);data.extend(struct.pack('<H',2))
        floats([2,.5]);floats([0,.25]);floats([3,.25])
        data.extend((0,2,4,6))
        data.extend((0,4,8))
        if len(data)%2:data.append(0)
        data.extend(struct.pack('>4H',0,100,200,300))
        return bytes(data)

    def test_all_nine_encodings_and_half_rate_interpolation(self):
        packet=CurvePacket(self.fixture(),4)
        self.assertEqual(packet.sample(0),[0,2,4,4,4,5,2,0,3])
        self.assertEqual(packet.sample(2),[0,2,10,18,26,7,4,1,53])
        self.assertEqual(packet.sample(3),[0,2,13,31,58,6,5,1.5,78])
        self.assertEqual(packet.sample(100),packet.sample(3))

    def test_truncated_packets(self):
        for data in (b'',b'\x45',self.fixture()[:-1]):
            with self.assertRaises(ValueError):CurvePacket(data,4)

    def test_original_animation_hash(self):
        self.assertEqual(game_hash(b'RNORM_FWD_CYC'),0x05d2dff3)
        self.assertEqual(game_hash(b'BLOCK'),0x0047137b)
        self.assertEqual(game_hash(b'BLOCK\0ignored'),0x0047137b)


if __name__=='__main__':unittest.main()
