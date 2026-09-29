"""MDLPS2.BIG / MACTXP.BIG with Sam's Equip Gear members (SamWardrobeParts.cs -> sam-assets/w/).

The original BIG directories have only ~0.9/1.7 KB of padding before the first payload, too little for Sam's
~40 model files and ~60 texture packs. The directory is therefore rebuilt larger and every original payload moves
by the same sector-aligned amount: payload bytes and member order are unchanged, only the directory offsets change.
New members follow the originals. The result is checked by decoding every member.
  python3 tools/sam_ps2/rebuild_big.py
"""
from pathlib import Path
import struct, json, hashlib, sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from inspect_disc import Disc
from compare_character_assets import big_members
ROOT = Path(__file__).resolve().parents[2]

def members_raw(data):
    count, end = struct.unpack_from('>II', data, 8); at = 16; out = []
    for _ in range(count):
        off, size = struct.unpack_from('>II', data, at); z = data.index(0, at + 8, end)
        out.append((data[at + 8:z].decode('ascii'), off, size)); at = z + 1
    return out, data[at:end]

def rebuild(original, added):
    old, footer = members_raw(original)
    names = {n.lower() for n, _, _ in old}; assert not names & {n.lower() for n in added}
    first = min(o for _, o, s in old if s)
    directory_size = 16 + sum(8 + len(n) + 1 for n, _, _ in old) + sum(8 + len(n) + 1 for n in added) + len(footer)
    new_first = (directory_size + 2047) // 2048 * 2048
    delta = max(0, new_first - first); delta = (delta + 2047) // 2048 * 2048
    body = bytearray(original[first:])            # original payload region, relative layout kept
    entries = [(n, (o + delta) if s else o, s) for n, o, s in old]
    out = bytearray(first + delta); out += body
    for n, payload in sorted(added.items()):
        out += bytes((-len(out)) % 2048); entries.append((n, len(out), len(payload))); out += payload
    d = bytearray()
    for n, o, s in entries: d += struct.pack('>II', o, s) + n.encode('ascii') + b'\0'
    end = 16 + len(d) + len(footer); assert end <= first + delta
    out[:16] = b'BIGF' + struct.pack('<I', len(out)) + struct.pack('>II', len(entries), end)
    out[16:end] = d + footer
    out[end:first + delta] = bytes(first + delta - end)
    dec = dict(big_members(bytes(out))); orig = dict(big_members(original))
    assert all(dec[n] == b for n, b in orig.items()) and all(dec[n] == b for n, b in added.items())
    return bytes(out), dict(original_members=len(orig), new_members=len(added), payload_shift=delta, directory_end=end)

if __name__ == '__main__':
    folder = ROOT / 'local/sam-ps2/roster/sam-assets'; w = folder / 'w'
    d = Disc(Path.home()/'Downloads/SSX 3 (USA).iso'); report = {}
    sets = {'MDLPS2.BIG': {p.name: p.read_bytes() for p in w.glob('*.mpf')},
            'MACTXP.BIG': {**{p.name: p.read_bytes() for p in w.glob('*.ssh')}, 'sam_icons.ssh': (folder / 'sam_icons.ssh').read_bytes()}}
    for name, added in sets.items():
        result, checks = rebuild(d.file('DATA/CHAR/' + name), added)
        (folder / name).write_bytes(result); checks['sha256'] = hashlib.sha256(result).hexdigest(); report[name] = checks
    (folder / 'archive-verification.json').write_text(json.dumps(report, indent=2) + '\n'); print(json.dumps(report, indent=2))
