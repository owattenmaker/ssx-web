#!/usr/bin/env python3
"""Compare dimensions/encodings of matching character textures on both discs."""
import json
import struct
from collections import Counter
from pathlib import Path
from compare_character_assets import big_members
from inspect_disc import Disc, inspect_big
from world_assets import refpack


def texture_headers(data):
    if data[:2] == b'\x10\xfb':
        data = refpack(data)
    if data[:4] not in (b'SHPS', b'SHPG'):
        raise ValueError(f'Unexpected texture container {data[:4]!r}')
    endian = '>' if data[:4] == b'SHPG' else '<'
    count, = struct.unpack_from(endian+'I', data, 8)
    if 16+count*8 > len(data):
        raise ValueError('Truncated texture directory')
    result = []
    for i in range(count):
        p = 16+i*8
        name = data[p:p+4].decode('ascii')
        offset, = struct.unpack_from(endian+'I', data, p+4)
        width, height = struct.unpack_from(endian+'HH', data, offset+4)
        result.append(dict(name=name, width=width, height=height, encoding=data[offset]))
    return result


def main():
    folder = Path('local/gamecube/disc/files/data/char')
    from disc_paths import ps2_iso;disc = Disc(ps2_iso())
    rows, counts = [], Counter()
    try:
        for entry in disc.entries:
            if '/CHAR/' not in entry['path'] or 'TX' not in entry['path'] or not entry['path'].endswith('.BIG'):
                continue
            counterpart = folder / Path(entry['path']).name.lower().replace('txp','txn')
            if not counterpart.exists():
                # Retail USA PS2 also uses TXN naming for some archives.
                continue
            gc = {Path(n).stem.casefold():d for n,d in big_members(counterpart.read_bytes())}
            for member in inspect_big(disc,entry)['files']:
                key = Path(member['path']).stem.casefold()
                if key not in gc or not member['path'].lower().endswith('.ssh'):
                    continue
                ps = texture_headers(disc.read(member['disc_offset'],member['size']))
                ngc = {i['name']:i for i in texture_headers(gc[key])}
                for image in ps:
                    other = ngc.get(image['name'])
                    if not other:
                        continue
                    a,b = image['width']*image['height'],other['width']*other['height']
                    verdict = 'same_dimensions' if (image['width'],image['height'])==(other['width'],other['height']) else 'gamecube_more_pixels' if b>a else 'ps2_more_pixels' if a>b else 'different_shape'
                    counts[verdict] += 1
                    rows.append(dict(archive=entry['path'],resource=key,ps2=image,gamecube=other,comparison=verdict))
    finally:
        disc.close()
    report = dict(note='Dimensions measure resolution, not compression quality or visual fidelity. No source preference is inferred automatically.',
                  comparison=dict(counts),textures=rows)
    Path('local/assets/texture-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(compared=len(rows),**counts)))


if __name__ == '__main__':
    main()
