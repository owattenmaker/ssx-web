#!/usr/bin/env python3
"""Export the original in-game loading screen assets for the browser (read-only).

Source (user's own disc, never modified):
* DATA/UI/GL.LUI + DATA/UI/GL_1.SSH: the "game load" front-end package (cGameLoadScreen 0x232738).
  GL_1 pages: 0 load art3 (MCOMM, unused here), 1 Widg (widgets/icons), 2 trees + sky swatch,
  3 mountain, 4 help (DualShock 2 + button icons), 5 FE title swoosh / SSX 3 logo.
  The GL.LUI object table (name hash, flags, u0 v0 u1 v1) gives each sprite's page (flags bits 8..15) and UVs.
* DATA/LOCALE/CMNAMER.LOC / FEAMER.LOC ('Default') / OVAMER.LOC ('Hints and Tips'): the screen strings (the 110ctrl_load "Basic Controls" labels),
  "Loading...", and the fifteen kT_FEHINTTitle%d / kT_FEHINTDES%d load hints (101QPMPHints, 0x245950).
The output (web/public/assets/LOADING/) is git-ignored like every other extracted asset.
See docs/loading-screen.md for the selection rules, timing and addresses.
"""
import argparse, hashlib, json, struct, sys, zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/sam_ps2'))
from inspect_disc import Disc  # noqa: E402
from world_assets import refpack  # noqa: E402
from lui_screen import Screen  # noqa: E402
import loc_file  # noqa: E402

from disc_paths import ps2_iso;ISO = ps2_iso()
PAGES = {1: 'Widg', 2: 'trees', 3: 'mountain', 4: 'help', 5: 'title'}


def png(width, height, rgba):
    def chunk(tag, body): return struct.pack('>I', len(body)) + tag + body + struct.pack('>I', zlib.crc32(tag + body) & 0xffffffff)
    rows = b''.join(b'\0' + rgba[y * width * 4:(y + 1) * width * 4] for y in range(height))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))


def shps_pages(data):
    """Every 8-bit CLUT image of a SHPS pack. Indices go through the PSMT8 CSM1 CLUT swizzle (short CLUTs too:
    the tree page's 98-entry CLUT is only solid that way); GS alpha 0..128 doubled."""
    if data[:4] != b'SHPS': raise ValueError('Expected a SHPS container')
    pages = []
    for i in range(struct.unpack_from('<I', data, 8)[0]):
        name = data[16 + 8 * i:20 + 8 * i].decode('latin1'); d = data[struct.unpack_from('<I', data, 20 + 8 * i)[0]:]
        kind, size = d[0], int.from_bytes(d[1:4], 'little'); width, height = struct.unpack_from('<HH', d, 4)
        if kind != 2: raise ValueError(f'Unsupported SHPS image kind {kind}')
        p = d[size:]
        while p[0] != 0x21:                       # skip to the 32-bit CLUT chunk
            step = int.from_bytes(p[1:4], 'little')
            if not step: raise ValueError('SHPS image without CLUT')
            p = p[step:]
        pw, ph = struct.unpack_from('<HH', p, 4); palette = p[16:16 + 4 * pw * ph]
        out = bytearray()
        for index in d[16:16 + width * height]:
            e = (index & 0xe7) | ((index & 8) << 1) | ((index & 16) >> 1)
            r, g, b, a = palette[4 * e:4 * e + 4] if 4 * e + 4 <= len(palette) else (0, 0, 0, 0)
            out += bytes((r, g, b, min(255, 2 * a)))
        pages.append(dict(name=name, width=width, height=height, rgba=bytes(out)))
    return pages


CTRL_LOAD = 0x0FAEE014   # GL.LUI screen "110ctrl_load" (Basic Controls), cGameLoadStateConquer 0x245730
KIND = {16: 'group', 17: 'sprite', 23: 'text', 24: 'shape'}


def lui_screens(data):
    """GL.LUI screen table: (name hash, offset) pairs; each screen is a refpack-compressed lui_screen.Screen."""
    _, _, _, table, objects, _ = struct.unpack_from('<6I', data, 0)
    entries = [struct.unpack_from('<II', data, table + 4 + 8 * i) for i in range(struct.unpack_from('<I', data, table)[0])]
    out = {}
    for i, (name, offset) in enumerate(entries):
        end = entries[i + 1][1] if i + 1 < len(entries) else objects - table
        out[name] = refpack(data[table + offset:table + end])
    return out


def lui_animations(data):
    """GL.LUI u1 table: 0x50 records (u16 type, u16 frames, u32 tracks, u32 name, u32 size), each track a 0x51 line
    (u16 type, u16 property id, u16 size, u32 n, then n s16 values with n-1 u16 frame counts interleaved)."""
    table = struct.unpack_from('<I', data, 8)[0]; out = {}; at = table + 4
    for _ in range(struct.unpack_from('<I', data, table)[0]):
        kind, frames, count, name, size = struct.unpack_from('<HHIII', data, at)
        if kind != 0x50: raise ValueError('Unexpected LUI animation record')
        q = at + 16; tracks = {}
        for _ in range(count):
            line, prop, length = struct.unpack_from('<HHH', data, q)
            if line != 0x51: raise ValueError('Unexpected LUI animation track')
            n = struct.unpack_from('<I', data, q + 6)[0]
            tracks[prop] = [[struct.unpack_from('<h', data, q + 10 + 4 * j)[0], struct.unpack_from('<H', data, q + 12 + 4 * j)[0] if j < n - 1 else 0] for j in range(n)]
            q += length
        if q != at + size: raise ValueError('LUI animation size mismatch')
        out[f'{name:08x}'] = dict(frames=frames, tracks=tracks); at = q
    return out


def props(record):
    """State record 0x21: u32 element, u32 n, then n (u16 property id, s16 value)."""
    n = struct.unpack_from('<I', record, 8)[0]
    return {pid: value for pid, value in (struct.unpack_from('<Hh', record, 12 + 4 * j) for j in range(n))}


def screen_layout(data, sprites_by_hash, strings):
    """Flatten a LUI screen: definitions (type, name, flags, children / sprite / text), the frame-0 state
    (0x21 property records or 0x20 animation bindings) and the timeline events of the later states."""
    sc = Screen.decode(data)
    elements, parent = [], {}
    for index, (d, s) in enumerate(zip(sc.definitions, sc.states[0].records)):
        kind, size = struct.unpack_from('<HH', d, 0); name, flags = struct.unpack_from('<II', d, 4); tail = d[32:]
        e = dict(name=f'{name:08x}', kind=KIND[kind], layer=flags & 0x3f, flags=flags)
        if kind == 16:
            n = struct.unpack_from('<I', tail, 0)[0]; e['children'] = [f'{c:08x}' for c in struct.unpack_from('<%dI' % n, tail, 4)]
        elif kind == 17:
            e['sprite'] = sprites_by_hash[struct.unpack_from('<I', tail, 4)[0]]
        elif kind == 23:
            key = struct.unpack_from('<I', tail, 4)[0]; e['text'] = strings.get(key); e['text_hash'] = f'{key:08x}'
        record = struct.unpack_from('<H', s, 0)[0]
        if record == 0x21:                               # (element, n, properties)
            if struct.unpack_from('<I', s, 4)[0] != name: raise ValueError('State record out of order')
            e['props'] = props(s)
        elif record == 0x20:                             # (animation, element, mode): the element is driven by it
            anim, element, mode = struct.unpack_from('<III', s, 4)
            if element != name: raise ValueError('State record out of order')
            e['anim'] = dict(hash=f'{anim:08x}', mode=mode)
        else: raise ValueError('Unexpected LUI state record')
        elements.append(e)
    for e in elements:
        for c in e.get('children', []): parent[c] = e['name']
    for e in elements: e['parent'] = parent.get(e['name'])
    events = []
    for state in sc.states[1:]:
        for r in state.records:
            record = struct.unpack_from('<H', r, 0)[0]
            if record == 0x20:
                anim, element, mode = struct.unpack_from('<III', r, 4)
                events.append(dict(frame=state.flags, anim=f'{anim:08x}', element=f'{element:08x}', mode=mode))
            elif record == 0x21:
                events.append(dict(frame=state.flags, element=f'{struct.unpack_from("<I", r, 4)[0]:08x}', props=props(r)))
    return elements, events


def lui_objects(data):
    """GL.LUI header: magic, version, u1, screens, objects, fonts offsets; object = hash, flags, u0 v0 u1 v1."""
    _, _, _, _, objects, _ = struct.unpack_from('<6I', data, 0)
    out = []
    for i in range(struct.unpack_from('<I', data, objects)[0]):
        name, flags, u0, v0, u1, v1 = struct.unpack_from('<II4f', data, objects + 4 + 24 * i)
        out.append(dict(hash=f'{name:08x}', page=(flags >> 8) & 0xff, uv=[u0, v0, u1, v1]))
    return out


# Strings the browser adds to the screen (keyboard panel, extra DualShock labels, hint line).
LABELS = {
    'turn_spin_flip': 0x079983a0, 'turn': 0x0df8cdae, 'jump': 0x0df86d60, 'boost_tweak': 0x0ac2df5b, 'grab_board': 0x0994d6a4,
    'reset': 0x0f8fda64, 'hand_plant': 0x07281774, 'board_press': 0x04b36453, 'pause': 0x0f819f35, 'loading': 0x0efae167,
    'default': 0x098d2854, 'hints_and_tips': 0x08c976b3,
}


def export(output):
    disc = Disc(ISO)
    try:
        ssh = disc.file('DATA/UI/GL_1.SSH'); lui = disc.file('DATA/UI/GL.LUI')
        strings = {}
        for name in ('FEAMER', 'OVAMER', 'CMNAMER'): strings.update(loc_file.entries(disc.file(f'DATA/LOCALE/{name}.LOC')))
    finally:
        disc.close()
    output.mkdir(parents=True, exist_ok=True)
    pages = shps_pages(ssh)
    for index in PAGES:
        p = pages[index]; (output / f'GL_1-{index}.png').write_bytes(png(p['width'], p['height'], p['rgba']))
    sprites = {}
    for o in lui_objects(lui):
        if o['page'] not in PAGES: continue
        size = pages[o['page']]['width']; u0, v0, u1, v1 = (x * size for x in o['uv'])
        sprites[int(o['hash'], 16)] = dict(hash=o['hash'], page=f'GL_1-{o["page"]}', sx=round(u0, 3), sy=round(v0, 3), sw=round(u1 - u0, 3), sh=round(v1 - v0, 3))
    elements, events = screen_layout(lui_screens(lui)[CTRL_LOAD], sprites, strings)
    animations = lui_animations(lui)
    used = {e['anim']['hash'] for e in elements if 'anim' in e} | {v['anim'] for v in events if 'anim' in v}
    hints = [dict(index=n, title=strings[loc_file.name_hash(f'kT_FEHINTTitle{n}')], body=strings[loc_file.name_hash(f'kT_FEHINTDES{n}')]) for n in range(1, 16)]
    result = dict(
        provenance=dict(gl_1_ssh_sha256=hashlib.sha256(ssh).hexdigest(), gl_lui_sha256=hashlib.sha256(lui).hexdigest(),
                        screen='GL.LUI 110ctrl_load (0x0FAEE014), cGameLoadStateConquer vtable 0x47C7A8 (selected by cGameLoadState init 0x232E20)',
                        hints='kT_FEHINTTitle%d/kT_FEHINTDES%d (101QPMPHints 0x245950): n = counter+1, 12 skipped, counter = (counter+1) % 15 (byte gp-0x4B8)'),
        coordinates='LUI 640x480 frame; props: 0 x, 1 y, 3/4 pivot, 5 rotation deg, 6 w, 7 h, 9/10 scale %, 12 anchor, 13 A, 14 R, 15 G, 16 B; shapes: 4 vertices (21+9k x, 22+9k y, 26..29+9k ARGB)',
        pages={f'GL_1-{i}': dict(name=pages[i]['name'], size=pages[i]['width']) for i in PAGES},
        elements=elements, events=events, animations={k: animations[k] for k in sorted(used)},
        strings={k: strings[v] for k, v in LABELS.items()},
        hints=hints, hint_skip=12,
        # Measured on ARMSX2 (docs/loading-screen.md): Single Event Snow Jam, frames after the screen's first frame -> percent.
        timing=dict(original_frames=815, percent_curve=[[0, 0], [20, 2], [120, 17], [240, 27], [400, 52], [520, 97], [540, 98], [800, 98], [810, 100]]),
    )
    (output / 'loading.json').write_text(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--output', type=Path, default=ROOT / 'web/public/assets/LOADING')
    r = export(p.parse_args().output)
    print(f"Loading screen: {len(r['elements'])} elements, {len(r['animations'])} animations, {len(r['events'])} events, {len(r['hints'])} hints -> {p.parse_args().output}")


if __name__ == '__main__':
    main()
