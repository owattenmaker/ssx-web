#!/usr/bin/env python3
"""Rival-challenge rider FX textures (docs/backcountry.md), read-only, from a Happiness rival savestate.

The renderer's resident FX textures are handles in renderer (*(gp-0x854)) +0xF50..; descriptors at
*(renderer+0x18F4) + handle*4 + 8 (name at +4, TEX0 at +0x38, texels +0x1C, palette +0x20), as tools/export_hud_glow.py.
  renderer+0xFFC 'beam' (64x64 PSMCT32): the rival locator beam, component RFX+0xB00 draw 0x2E3AF8.
  renderer+0xFAC 'exlm' (128x128 PSMT8, DATA/TEXTURES/PARTICLE.SSH): the '!' over the rival, component RFX+0xAF0 (0x2D5048).
Output (git-ignored): web/public/assets/UI/rival-beam.png, rival-exclaim.png (+ .json provenance).
  python3 tools/export_rival_fx.py [--state local/reference/pcsx2/happiness-glide.p2s]
"""
import argparse, hashlib, json, struct, zipfile, zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GP = 0x4A30F0


def png(width, height, rgba):
    def chunk(kind, data): return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    scan = b''.join(b'\0' + rgba[y * width * 4:(y + 1) * width * 4] for y in range(height))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', width, height, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(scan)) + chunk(b'IEND', b'')


def export(memory, slot, name, out):
    u = lambda p: struct.unpack_from('<I', memory, p)[0]
    renderer = u(GP - 0x854); handle = u(renderer + slot); d = u(u(renderer + 0x18F4) + handle * 4 + 8)
    if memory[d + 4:d + 8] != name.encode(): raise ValueError(f'renderer+{slot:#x} is {memory[d + 4:d + 8]}, expected {name}')
    tex0 = struct.unpack_from('<Q', memory, d + 0x38)[0]; w, h, psm = 1 << ((tex0 >> 26) & 15), 1 << ((tex0 >> 30) & 15), (tex0 >> 20) & 63
    rgba = bytearray()
    if psm == 0:     # PSMCT32, GS alpha 0..128
        texels = memory[u(d + 0x1C):u(d + 0x1C) + w * h * 4]
        for k in range(w * h): r, g, b, a = texels[4 * k:4 * k + 4]; rgba += bytes((r, g, b, min(255, a * 2)))
    elif psm == 27:  # PSMT8 with a CSM1 palette (index bits 3/4 swapped)
        texels = memory[u(d + 0x1C):u(d + 0x1C) + w * h]; palette = memory[u(d + 0x20):u(d + 0x20) + 1024]
        for i in texels:
            s = (i & 0xE7) | ((i & 8) << 1) | ((i & 16) >> 1); r, g, b, a = palette[4 * s:4 * s + 4]; rgba += bytes((r, g, b, min(255, a * 2)))
    else: raise ValueError(f'{name}: unsupported psm {psm}')
    (out.with_suffix('.png')).write_bytes(png(w, h, bytes(rgba)))
    out.with_suffix('.json').write_text(json.dumps(dict(entry=name, renderer_slot=hex(slot), runtime_handle=handle, width=w, height=h, psm=psm,
                                                        texels_sha256=hashlib.sha256(bytes(rgba)).hexdigest()), indent=1) + '\n')
    print(name, w, h, psm, '->', out.with_suffix('.png'))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--state', default=str(ROOT / 'local/reference/pcsx2/happiness-glide.p2s')); a = p.parse_args()
    memory = zipfile.ZipFile(a.state).read('eeMemory.bin')
    ui = ROOT / 'web/public/assets/UI'; ui.mkdir(parents=True, exist_ok=True)
    export(memory, 0xFFC, 'beam', ui / 'rival-beam'); export(memory, 0xFAC, 'exlm', ui / 'rival-exclaim')


if __name__ == '__main__':
    main()
