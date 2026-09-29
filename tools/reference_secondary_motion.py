#!/usr/bin/env python3
"""Original120378 secondary-motion (hair channels 3..5) rider profile from a savestate."""
import struct

def extract_secondary_motion(memory, rider=0x14701a0):
    u = lambda o: struct.unpack_from('<I', memory, rider + o)[0]
    i = lambda o: struct.unpack_from('<i', memory, rider + o)[0]
    q = lambda o: list(struct.unpack_from('<4f', memory, rider + o))
    return dict(provenance='0x120378: rider+86C slot, +940/+970/+9A0 enable, +8A8/+8AC/+89C compiled world bone, +950/+980/+9B0 and +960/+990/+9C0 local rotations',
                slot=i(0x86C), enabled=[u(o) != 0 for o in (0x940, 0x970, 0x9A0)], bones=[i(o) for o in (0x8A8, 0x8AC, 0x89C)],
                forward=[q(o) for o in (0x950, 0x980, 0x9B0)], reverse=[q(o) for o in (0x960, 0x990, 0x9C0)])

if __name__ == '__main__':
    import json, sys, zipfile
    with zipfile.ZipFile(sys.argv[1]) as z: print(json.dumps(extract_secondary_motion(z.read('eeMemory.bin')), indent=1))
