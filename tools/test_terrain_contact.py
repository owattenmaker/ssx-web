#!/usr/bin/env python3
"""Validate native analytic patch query against original retained contact states."""
import json
import struct
import subprocess
import zipfile
from pathlib import Path
from reference_probes import riders

def main():
    root=Path(__file__).resolve().parent.parent
    package=json.loads((root/'local/assets/native/ARA1/terrain.json').read_text())
    states=[root/'local/reference/pcsx2'/f'snow-jam-{x}.p2s' for x in ('glide','glide-1','glide-30-a','glide-120','brake-30','charge-15','jump-30','turn-left-30')]
    folder=root/'local/reference/terrain';folder.mkdir(parents=True,exist_ok=True)
    fixture=folder/'native-contact-fixture.txt'
    with fixture.open('w') as out:
        print(len(package['patches']),len(states),file=out)
        for patch in package['patches']:
            print(patch['resource_id'],*(x for c in patch['coefficients'] for x in c),patch['authored_surface_id'],file=out)
        for state in states:
            with zipfile.ZipFile(state) as z:memory=z.read('eeMemory.bin')
            rider=next(r for r in riders(memory) if r['kind']=='human');base=int(rider['address'],16)
            x,y,z=struct.unpack_from('<3f',memory,base+0x380);prior=[x,z,-y]
            resource=struct.unpack_from('<I',memory,base+0x430)[0]
            x,y,z=struct.unpack_from('<3f',memory,base+0x460)
            lateral=[0,0,0]
            if state.name=='snow-jam-turn-left-30.p2s':
                with zipfile.ZipFile(state.with_name('snow-jam-turn-left-29.p2s')) as archive:old=archive.read('eeMemory.bin')
                old_base=int(next(r for r in riders(old) if r['kind']=='human')['address'],16)
                lx,ly,lz=struct.unpack_from('<3f',old,old_base+0x3b0);lateral=[lx,lz,-ly]
            geometry=struct.unpack_from('<I',memory,base+0x780)[0];scale=struct.unpack_from('<f',memory,geometry+0x140)[0]
            print(*rider['native_position_m'],*prior,*rider['native_contact_normal'],resource,x/100,z/100,-y/100,*lateral,
                  rider['controls']['turn_1f0'],scale,rider['source_contact_distance_cm'],*struct.unpack_from('<2f',memory,base+0xaac),struct.unpack_from('<i',memory,base+0x438)[0],file=out)
    binary=folder/'terrain_contact_test'
    subprocess.run(['clang++','-std=c++20','-O2','-I'+str(root/'engine'),str(root/'tests/terrain_contact_test.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(fixture)],check=True)


if __name__=='__main__':main()
