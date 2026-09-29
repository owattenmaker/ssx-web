#!/usr/bin/env python3
"""Export the camera trigger volumes (world-stream record kind 17) of every location (read-only).

Source (user's own disc, never modified): DATA/WORLDS/BAM.BIG members bam.ssb (CBXS/CEND RefPack world stream,
tools/world_assets.py) and bam.sdb (location index). The location name is the course code the asset pipeline uses
(tools/import_world.py --location ARA1 -> web/public/assets/ARA1/) and a record's track byte is its SDB index. Each
location with camera triggers carries exactly one kind-17 record (rid 0) in its last chunk. The chunk dispatcher 0x3AAE68
(jump table 0x494FB0[17] -> 0x3AB320) hands it to 0x16CEC8 -> cCameraTriggerMan_streamIn 0x16CE30 (manager 0x4C5830),
which parses it with cCameraTriggerList_loadFromBuffer 0x16C228.

Record (little-endian u32 / f32; get_uint 0x2D2558, get_float 0x2D2510, get_t3Vector 0x2D25A0):
  u32 version (must be 7, readCookie 0x16C3A8); u32 list+0x10; u32 count; u32 list+0x8 (next id)   readHeader 0x16C3D0
  count x trigger (readTrigger 0x16C428, 0x24-byte object):
    u32 id (+0x0); u32 mask (+0x4: bit 0 replay 0x16D1D8, bit 1 in-game 0x16D1B8; 0 = never registered)
    volume (get_camvolume 0x172E18, 0x2C bytes): u32 type (0 ellipsoid 'Camera Sphere Trigger', 1 'Camera Box Trigger');
      f32 pos[3] (+0x0), f32 scale[3] (+0x18: radii / half extents), f32 rotZ (+0xC), rotX (+0x10), rotY (+0x14)
    enter action (+0xC), exit action (+0x10) (get_camaction 0x1712B0):
      0 switch  0x1715A0: f32 blend (+0xC); u32 camera (+0x8, mapped to a director type by jump table 0x45BF80)
      1 bounded 0x1714E8: f32 blend, distance, fov, lookHeight, pitchDegrees, lookAhead (+0xC..+0x20); u32 lookMode
                (+0x24); f32 lookPoint[3] (+0x28); then a bound object (get_camboundobj 0x171690, 0x2C/0x44 bytes):
                u32 type (0 ellipse, 1 box, 2 line, 3 point); 0/1/2: pos[3], scale[3], rotZ, rotX, rotY as the volume;
                2 adds a[3] (+0x2C), b[3] (+0x38); 3: pos[3] only
      2 spline  0x1715F8: f32 blend, fov (+0x10), param14 (+0x14 -> obj +0x3C0), duration (+0x18 -> obj +0x3B4),
                param1C (+0x1C -> obj +0x3C4); then get_camspline 0x171D58: pos[3], scale[3], rotZ, rotX, rotY, p0..p3[3]
      3 none    (nothing more)
  the payload is padded to its record size with 0xDEADBEEF words.

Runtime use (engine notes, not part of the data): 0x16D320 tests the human rider's point (rider+0x110) against the
registered volumes every mover tick; entering pushes the id on the per-view stack and runs the enter action, leaving pops
it and runs the exit action when the stack empties (or re-runs the new top's enter action). 0x16E1D8 turns an action into
0x162060(director, type, argument, rate) with rate = 1 when blend == 0, else (1 / blend) * (1 / 60): switch -> the mapped
type, bounded -> 0x5B (argument = the action), spline -> 0x5C.

Output: --out DIR (default: the replay-camera scratch directory) / <CODE>.json, one file per location with a record;
--assets: DIR/<CODE>/camera-triggers.json (compact) for the locations with replay triggers, the layout of web/public/assets
(web/replay_camera.inc replay_camera_triggers, docs/replay.md). Stage it outside web/public/assets first (shared tree).
Coordinates stay in PS2 source units (centimetres, Z up); angles are radians except pitchDegrees.
"""
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_disc import Disc, inspect_big, inspect_elf, EXPECTED_SHA1  # noqa: E402
from world_assets import refpack, records  # noqa: E402

from disc_paths import ps2_iso;DEFAULT_ISO = ps2_iso()
DEFAULT_OUT = ROOT / 'local/export/camera-triggers'  # scratch (git-ignored); copy into web/public/assets
RECORD_KIND = 17
VERSION = 7
PADDING = b'\xef\xbe\xad\xde'
SWITCH_TABLE, SWITCH_COUNT = 0x45BF80, 0x4C  # 0x16E304: sltiu camera, 0x4C; out of range -> type 0

VOLUME_TYPES = {0: 'ellipsoid', 1: 'box'}
BOUND_TYPES = {0: 'ellipse', 1: 'box', 2: 'line', 3: 'point'}


def director_name(t):
    """Algorithm built by the factory 0x15D078 (jump table 0x45B5C0) for a director request type."""
    names = {0x1: 'DEFAULT_4', 0x2: 'DEFAULT_2', 0x7: 'Idle', 0x9: 'Mid Right', 0xA: 'Mid Left', 0xB: 'Manual',
             0xD: 'Lazy', 0x20: 'Look Back', 0x29: 'Matrix', 0x3C: 'DEFAULT_2', 0x3E: 'DEFAULT_4', 0x42: 'SPOKE',
             0x44: 'POST_RACE_1', 0x4C: 'preferred (0x162060 maps 0x4C to director+0x24)', 0x5B: 'Bounded',
             0x5C: 'Spline'}
    if t in names:
        return names[t]
    if 0x12 <= t <= 0x19:
        return 'Direction ' + ('N', 'NW', 'W', 'SW', 'S', 'SE', 'E', 'NE')[t - 0x12]
    if 0x21 <= t <= 0x28 or 0x5E <= t <= 0x61:
        return 'Relative'
    if t in (0x0, 0x3, 0x4, 0x5, 0x6, 0x8, 0xC, 0xE, 0xF, 0x10, 0x11, 0x3D, 0x3F, 0x40, 0x41, 0x4A) \
            or 0x1A <= t <= 0x1F or 0x2A <= t <= 0x3B:
        return 'DEFAULT_3'
    return 'none (factory entry has no algorithm)'


def elf_reader(elf):
    segments = inspect_elf(elf)['load_segments']

    def word(address):
        for s in segments:
            if s['address'] <= address < s['address'] + s['file_size'] - 3:
                return struct.unpack_from('<I', elf, s['offset'] + address - s['address'])[0]
        raise ValueError(f'Address {address:#x} outside the ELF image')
    return word


def switch_camera_types(elf):
    """Decode 0x16E1D8's switch jump table: each case is `b 0x16E544; addiu $a1, $zero, type`."""
    word, table = elf_reader(elf), {}
    for camera in range(SWITCH_COUNT):
        target = word(SWITCH_TABLE + camera * 4)
        first, second = word(target), word(target + 4)
        if first == 0x0000282D:                    # daddu $a1, $zero, $zero (0x16E540)
            table[camera] = 0
        elif second == 0x24A5FFFF:                 # addiu $a1, $a1, -1 (0x16E430)
            table[camera] = camera - 1
        elif second & 0xFFFF0000 == 0x24050000:    # addiu $a1, $zero, imm
            table[camera] = second & 0xFFFF
        else:
            raise ValueError(f'Unexpected switch case at {target:#x}')
    return table


def ssb_chunks(data):
    """tools/world_assets.world_chunks over an in-memory stream: RefPack blocks concatenate until CEND."""
    pos, assembled = 0, bytearray()
    while pos < len(data):
        tag, size = data[pos:pos + 4], struct.unpack_from('<I', data, pos + 4)[0]
        if tag not in (b'CBXS', b'CEND') or not 8 < size <= 16 * 1024 * 1024 or pos + size > len(data):
            raise ValueError(f'Invalid world block at {pos}')
        assembled.extend(refpack(data[pos + 8:pos + size]))
        pos += size
        if tag == b'CEND':
            yield bytes(assembled)
            assembled.clear()
    if assembled:
        raise ValueError('Unterminated world chunk')


def sdb_locations(data):
    """tools/world_assets.locations over bytes: 88-byte records after an 80-byte header."""
    count = struct.unpack_from('<I', data, 8)[0]
    if len(data) < 80 + count * 88:
        raise ValueError('Truncated SDB locations')
    result = []
    for i in range(count):
        p = 80 + i * 88
        name = data[p:p + 16].split(b'\0')[0].decode('ascii')
        result.append(dict(name=name, chunk_end=struct.unpack_from('<I', data, p + 24)[0]))
    return result


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def u32(self):
        if self.pos + 4 > len(self.data):
            raise ValueError('Truncated camera trigger record')
        value = struct.unpack_from('<I', self.data, self.pos)[0]
        self.pos += 4
        return value

    def f32(self):
        if self.pos + 4 > len(self.data):
            raise ValueError('Truncated camera trigger record')
        value = struct.unpack_from('<f', self.data, self.pos)[0]
        self.pos += 4
        return value

    def vec3(self):
        return [self.f32(), self.f32(), self.f32()]

    def transform(self):
        pos, scale = self.vec3(), self.vec3()
        return dict(pos=pos, scale=scale, rot=dict(z=self.f32(), x=self.f32(), y=self.f32()))


def bound_object(r):
    code = r.u32()
    if code not in BOUND_TYPES:
        raise ValueError(f'Unknown bound object type {code}')
    bound = dict(type=BOUND_TYPES[code], typeCode=code)
    if code == 3:
        bound['pos'] = r.vec3()
    else:
        bound.update(r.transform())
        if code == 2:
            bound['a'], bound['b'] = r.vec3(), r.vec3()
    return bound


def action(r, switch_types):
    code = r.u32()
    if code == 0:
        a = dict(kind='switch', kindCode=0, blendSeconds=r.f32(), camera=r.u32())
        a['directorType'] = switch_types.get(a['camera'], 0)
    elif code == 1:
        a = dict(kind='bounded', kindCode=1, blendSeconds=r.f32(), distance=r.f32(), fov=r.f32(), lookHeight=r.f32(),
                 pitchDegrees=r.f32(), lookAhead=r.f32(), lookMode=r.u32(), lookPoint=r.vec3(), directorType=0x5B)
        a['bound'] = bound_object(r)
    elif code == 2:
        a = dict(kind='spline', kindCode=2, blendSeconds=r.f32(), fov=r.f32(), param14=r.f32(), durationSeconds=r.f32(),
                 param1C=r.f32(), directorType=0x5C)
        spline = r.transform()
        spline['points'] = [r.vec3() for _ in range(4)]
        a['spline'] = spline
    elif code == 3:
        return dict(kind='none', kindCode=3)
    else:
        raise ValueError(f'Unknown camera action kind {code}')
    a['directorName'] = director_name(a['directorType'])
    return a


def decode(payload, switch_types):
    r = Reader(payload)
    version = r.u32()
    if version != VERSION:
        raise ValueError(f'Camera trigger version {version}, expected {VERSION}')
    field10 = struct.unpack_from('<f', payload, 4)[0]
    r.pos = 8
    count, next_id = r.u32(), r.u32()
    triggers = []
    for index in range(count):
        trigger_id, mask = r.u32(), r.u32()
        code = r.u32()
        if code not in VOLUME_TYPES:
            raise ValueError(f'Unknown trigger volume type {code}')
        volume = dict(type=VOLUME_TYPES[code], typeCode=code, **r.transform())
        triggers.append(dict(index=index, id=trigger_id, mask=mask, replay=bool(mask & 1), inGame=bool(mask & 2),
                             volume=volume, enter=action(r, switch_types), exit=action(r, switch_types)))
    tail = payload[r.pos:]
    if len(tail) % 4 or any(tail[i:i + 4] != PADDING for i in range(0, len(tail), 4)):
        raise ValueError('Camera trigger record has undecoded bytes')
    return dict(version=version, field10=field10, count=count, nextId=next_id), triggers, r.pos


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--iso', type=Path, default=DEFAULT_ISO)
    ap.add_argument('--out', type=Path, default=DEFAULT_OUT)
    ap.add_argument('--assets', action='store_true', help='write DIR/<CODE>/camera-triggers.json (compact), replay locations only')
    args = ap.parse_args()
    disc = Disc(args.iso)
    try:
        elf = disc.file('SLUS_207.72')
        if hashlib.sha1(elf).hexdigest() != EXPECTED_SHA1:
            raise ValueError('This exporter requires the verified USA PS2 release')
        archive = next(e for e in disc.entries if e['path'] == 'DATA/WORLDS/BAM.BIG')
        members = {Path(e['path']).name.lower(): e for e in inspect_big(disc, archive)['files']}
        ssb = disc.read(members['bam.ssb']['disc_offset'], members['bam.ssb']['size'])
        sdb = disc.read(members['bam.sdb']['disc_offset'], members['bam.sdb']['size'])
    finally:
        disc.close()
    switch_types = switch_camera_types(elf)
    locs = sdb_locations(sdb)
    provenance = dict(tool='tools/export_camera_triggers.py', iso=args.iso.name, bam_big_member='data/worlds/bam.ssb',
                      ssb_sha256=hashlib.sha256(ssb).hexdigest(), sdb_sha256=hashlib.sha256(sdb).hexdigest())
    found = {}
    for chunk_index, chunk in enumerate(ssb_chunks(ssb)):
        for kind, track, rid, payload in records(chunk):
            if kind != RECORD_KIND:
                continue
            if track >= len(locs):
                raise ValueError(f'Camera trigger record for unknown location {track}')
            first = locs[track - 1]['chunk_end'] + 1 if track else 0
            if not first <= chunk_index <= locs[track]['chunk_end']:
                raise ValueError(f'Camera trigger record of {locs[track]["name"]} outside its chunks')
            code = locs[track]['name']
            if code in found:
                raise ValueError(f'{code} has more than one camera trigger record')
            header, triggers, used = decode(payload, switch_types)
            found[code] = dict(
                location=code, sdbIndex=track, chunk=chunk_index, recordKind=RECORD_KIND, rid=rid,
                payloadBytes=len(payload), decodedBytes=used,
                provenance=dict(provenance, payload_sha256=hashlib.sha256(payload).hexdigest()),
                units='PS2 source units: centimetres, world space, Z up (no native Y-up / metre conversion); '
                      'angles in radians except pitchDegrees; blendSeconds 0 = instant cut',
                conventions=dict(
                    mask='bit 0: registered while a replay runs (0x16D1D8); bit 1: registered in game (0x16D1B8)',
                    ellipsoid='inside when |S^-1 Rz(rot.z) Ry(rot.y) Rx(rot.x) (p - pos)|^2 <= 1 (0x172278; column '
                              'vectors, right-handed axis rotations, S = diag(scale))',
                    box='inside when |q_i| <= scale_i for q = Rx(rot.x) Rz(rot.z) (p - pos); rot.y unused (0x1728F8)',
                    bounded='director type 0x5B (ctor 0x174190); eye = bound point (point) or the closest point of '
                            'segment a-b (line, 0x16FBB8); look = lookPoint when lookMode == 1, else head + forward * '
                            'lookAhead + Z * lookHeight; fov = horizontal half angle',
                    switchCamera='camera -> directorType through the 0x16E1D8 jump table 0x45BF80 (see switchCameraTypes)'),
                header=header,
                switchCameraTypes={str(k): v for k, v in sorted(switch_types.items())},
                triggers=triggers)
    if not found:
        raise ValueError('No camera trigger records found')
    args.out.mkdir(parents=True, exist_ok=True)
    for code, result in found.items():
        if not args.assets:
            (args.out / f'{code}.json').write_text(json.dumps(result, indent=1))
        elif any(x['replay'] for x in result['triggers']):
            (args.out / code).mkdir(exist_ok=True)
            (args.out / code / 'camera-triggers.json').write_text(json.dumps(result, separators=(',', ':')))
    summary = []
    for code, result in found.items():
        t = result['triggers']
        kinds = {k: sum(1 for x in t if x['enter']['kind'] == k) for k in ('switch', 'bounded', 'spline', 'none')}
        summary.append(f"{code} {len(t)} (replay {sum(x['replay'] for x in t)}, in-game {sum(x['inGame'] for x in t)}, "
                       f"enter switch {kinds['switch']} / bounded {kinds['bounded']} / spline {kinds['spline']})")
    print('camera triggers: ' + '; '.join(summary) + f' -> {args.out}')


if __name__ == '__main__':
    main()
