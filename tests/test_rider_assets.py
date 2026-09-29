"""Original rider package format regressions; no owned assets required."""
import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from rider_assets import decode_rider_texture,read_bolt_entries

class RiderAssetFormats(unittest.TestCase):
    def test_rgb5a3_alpha_and_tile_placement(self):
        header=bytearray(16);header[0]=21
        struct.pack_into('>HH',header,4,8,4)
        # First4x4 block opaque red; second block green with3/7 alpha.
        data=bytes(header)+struct.pack('>16H',*([0xfc00]*16))+struct.pack('>16H',*([0x30f0]*16))
        width,height,rgba=decode_rider_texture(data)
        self.assertEqual((width,height),(8,4))
        for y in range(4):
            for x in range(8):
                at=(y*8+x)*4
                self.assertEqual(tuple(rgba[at:at+4]),(255,0,0,255) if x<4 else (0,255,0,109))
        with self.assertRaises(ValueError):decode_rider_texture(data[:-1])

    def test_bolt_tables_and_bounded_strings(self):
        item=bytearray(56);item[0]=3
        struct.pack_into('>2h',item,4,142,9)
        struct.pack_into('>I',item,20,1)
        strings=b'Bedhead\0'
        data=struct.pack('>2I',910,1)+item+struct.pack('>4I',0,0,0,len(strings))+strings
        entries=read_bolt_entries(data)
        self.assertEqual((entries[0]['character'],entries[0]['id'],entries[0]['parent'],entries[0]['name']),(3,142,9,'Bedhead'))
        broken=bytearray(data);struct.pack_into('>I',broken,8+20,100)
        with self.assertRaises(ValueError):read_bolt_entries(broken)
        with self.assertRaises(ValueError):read_bolt_entries(data[:-1])

if __name__=='__main__':unittest.main()
