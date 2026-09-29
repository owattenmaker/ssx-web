#!/usr/bin/env python3
"""SSX 3 (PS2) in-engine cutscene ("NIS") script decoder (read-only; docs/cutscenes.md).

Reads DATA/SCRIPTS/SCDAT.BIG straight from the ISO (member offsets from local/disc/inventory.json),
decodes scmaster.dat / scmasterdbg.dat, every data/scripts/NNNNNNNN.big and scdat_<LOC>.big, and
writes one JSON per script (NNN.json) plus index.json and scfilter.json (research dump, default
local/cutscenes/isb/). tools/export_cutscenes.py uses the decode functions for the browser assets.

Usage: python3 tools/cutscene_isb.py [--iso PATH] [--out DIR]
"""
import argparse, json, math, os, struct, sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / 'tools'))
from animation_bank import read_bank, game_hash  # noqa: E402
from export_audio import parse_bank  # noqa: E402

MAGIC = 0x05E68A2A
EVENT_BITS = ['ARA1', 'BRA2', 'CRA3', 'DRA4', 'ERA5', 'FRA6', 'ASS1', 'DSS2', 'ESS3',
              'ABA1', 'CBA2', 'EBA3', 'BHP1', 'CHP2', 'EHP3']
FADE_TYPES = {0: 'none', 8: 'none', 1: 'colour fade', 2: 'wipe(2EBBA8,+1,+1)?', 3: 'wipe(2EBBA8)?', 4: 'wipe(2EBBA8)?',
              5: 'wipe(2EBBA8)?', 6: 'transition 2EBE20', 7: 'single-duration transition 2E4540'}
LOC_BITS = EVENT_BITS + ['ABC1', 'DBC2', 'EBC3', 'A', 'B', 'C', 'D', 'E', 'F']
FILTER_SLOTS = ['role0', 'role1', 'role2', 'human1', 'human2'] + [f'ai{i}' for i in range(1, 6)] + \
               [f'net{i}' for i in range(1, 6)] + [f'rider{i}' for i in range(6)]
CHARDB = ['moby', 'kaori', 'allegra', 'mac', 'zoe', 'griff', 'elise', 'nate', 'psymon', 'viggo']
PA_CUES = {0: 'PA_Sponsor_Intro', 1: 'PA_Rider_Intro', 2: 'PA_Rider_Race_Intro', 3: 'Medal_Run_Intro',
           5: 'PA_Finish_Line', 7: 'PA_Medals', 8: 'DJ_Hub_Weather', 0xA: 'DJ_Char_Stories', 0xC: 'DJ_Event_Intro',
           0xE: 'DJ_First_Spoke', 0xF: 'PA_Venue_Intro', 0x10: 'wait speech idle then advance NIS list', 0x11: 'no-op'}
MUSIC_CODES = {20: 'course change', 21: 'next song (fade 1s, force after 3s)', 22: 'next song', 23: 'next song',
               24: 'next song', 25: 'next song'}
TICK_HZ = 60  # script time unit = one game frame (verified: +299 ticks over 300 captured frames)

KIND_NAMES = {0: 'camera_manual', 1: 'camera_target', 2: 'camera_subject', 3: 'camera_algorithmic',
              4: 'camera_cuts', 5: 'actor', 7: 'audio_control'}
# Per-kind channel labels (filled from the engine analysis; see docs/cutscenes.md)
CHANNEL_LABELS = {
    # angles in degrees, positions in cm in the anchor frame, fov = half-horizontal angle (4:3)
    0: ['eye_x', 'eye_y', 'eye_z', 'target_x', 'target_y', 'target_z', 'roll', 'fov', 'shake_pos', 'shake_roll'],
    1: ['target_x', 'target_y', 'target_z', 'distance', 'pitch', 'yaw', 'roll', 'fov', 'shake_pos', 'shake_roll'],
    2: ['eye_x', 'eye_y', 'eye_z', 'subject_fwd_offset', 'subject_up_offset', 'roll', 'fov', 'shake_pos', 'shake_roll'],
    3: ['subject_fwd_offset', 'subject_up_offset', 'distance', 'pitch', 'yaw', 'roll', 'fov', 'shake_pos', 'shake_roll'],
    4: ['cuts'],
    5: ['anim_clips', 'sounds', 'speech_events', 'pos_x', 'pos_y', 'pos_z', 'rot_x', 'rot_y', 'rot_z'],
    7: ['stage_script_calls', 'music_codes', 'pa_dj_cues', 'sounds', 'weather_off'],
}


# ----------------------------------------------------------------------------------------------
# containers

def big_entries(data):
    if data[:4] != b'BIGF':
        raise ValueError('not BIGF')
    count = struct.unpack_from('>I', data, 8)[0]
    pos, out = 16, []
    for _ in range(count):
        off, size = struct.unpack_from('>II', data, pos); pos += 8
        end = data.index(b'\0', pos); name = data[pos:end].decode('latin1'); pos = end + 1
        out.append((name, data[off:off + size]))
    return out


def load_scdat(iso_path):
    inv = json.loads((REPO / 'local/disc/inventory.json').read_text())
    arch = next(a for a in inv['archives'] if a['path'].upper() == 'DATA/SCRIPTS/SCDAT.BIG')
    members = {}
    with open(iso_path, 'rb') as iso:
        for f in arch['files']:
            iso.seek(f['disc_offset']); members[f['path'].split('/')[-1]] = iso.read(f['size'])
    return members


# ----------------------------------------------------------------------------------------------
# scmaster

def decode_master(master, dbg):
    count = struct.unpack_from('<I', master, 0)[0]
    out = []
    for i in range(count):
        unk, h, off = struct.unpack_from('<III', master, 4 + 12 * i)
        name = dbg[off:dbg.index(b'\0', off)].decode()
        out.append(dict(number=i, name=name, hash=f'{h:08x}', hash_ok=game_hash(name.encode()) == h,
                        unk=unk, dbg_offset=off))
    return out


def decode_filter(data, master):
    """scfilter<LOC>.dat: u32 n(=28); n x {u32 count, u32 offset}; entries 0x30 {u32 script#, u16 mask[21], u16 pad}.
    mask[k] = CHARDB bit set that filter slot k's character must be in (0 = don't care); ScriptChoice 0x27B0C0."""
    n = struct.unpack_from('<I', data, 0)[0]
    lists = []
    for i in range(n):
        cnt, off = struct.unpack_from('<II', data, 4 + 8 * i)
        es = []
        for j in range(cnt):
            e = off + 0x30 * j
            sc = struct.unpack_from('<I', data, e)[0]; ms = struct.unpack_from('<21H', data, e + 4)
            req = {FILTER_SLOTS[k]: ('any' if m == 0x3ff else [CHARDB[c] for c in range(10) if m >> c & 1])
                   for k, m in enumerate(ms) if m}
            es.append(dict(script=sc, name=master[sc]['name'] if sc < len(master) else None, require=req))
        lists.append(es)
    return dict(list_count=n, lists=lists)


# ----------------------------------------------------------------------------------------------
# channels

def f32(v):
    """Exact float32 value; shortest decimal that round-trips through float32."""
    if math.isnan(v):
        return None
    for p in range(6, 12):
        r = float(f'{v:.{p}g}')
        if struct.pack('<f', r) == struct.pack('<f', v):
            return r
    return v


def curve_eval(keys, t):
    k = keys[0]
    for kk in keys:
        if kk['t'] <= t: k = kk
    u = t - k['t']
    a, b, c, d = k['abcd']
    return ((a * u + b) * u + c) * u + d


def ch_curve(p):
    n = struct.unpack_from('<I', p, 0)[0]
    keys = []
    for i in range(n):
        size, t, a, b, c, d = struct.unpack_from('<HH4f', p, 4 + 20 * i)
        assert size == 0x14
        keys.append(dict(t=t, abcd=[f32(a), f32(b), f32(c), f32(d)], value=f32(d)))
    # value at end of each segment (= start value of the next key unless there is a jump)
    for i, k in enumerate(keys):
        if i + 1 < len(keys):
            u = keys[i + 1]['t'] - k['t']; a, b, c, d = k['abcd']
            k['end_value'] = f32(((a * u + b) * u + c) * u + d)
    const = len(keys) == 1 or all(abs(k['abcd'][0]) + abs(k['abcd'][1]) + abs(k['abcd'][2]) < 1e-12
                                  and k['value'] == keys[0]['value'] for k in keys)
    return dict(type='curve', keys=keys, constant=keys[0]['value'] if const else None)


def _range_items(p):
    """Items shared by channel types 1 and 2 (engine copies them in 0x2730C0):
    {u16 size, u16 index, u16 t0, u16 t1, u32 value, u32 value2, extra[size-16]}"""
    count, sub = struct.unpack_from('<HH', p, 0)
    items, pos = [], 4
    for _ in range(count):
        size, idx, t0, t1, val, val2 = struct.unpack_from('<4HII', p, pos)
        items.append(dict(index=idx, t0=t0, t1=t1, value=val, value2=val2, extra=p[pos + 16:pos + size]))
        pos += size
    return sub, items


def ch_range(p):
    """type 1 (0x1c-byte items): extra = {f32 rate, u32 u3, u8[4] tag}"""
    sub, items = _range_items(p)
    for it in items:
        e = it.pop('extra')
        if len(e) >= 12:
            rate, blend = struct.unpack_from('<fI', e, 0)
            it['start_offset'] = struct.unpack('<i', struct.pack('<I', it.pop('value2')))[0]
            it.update(speed=f32(rate), blend_ticks=blend, tail=list(e[8:12]))
        elif e:
            it['extra'] = e.hex()
    return dict(type='ranges', pool_size=sub, items=items)


def ch_cuts(p):
    """type 2 (0x18- or 0x10-byte items): extra (8 bytes when present) = {u16 transition, u16, u16, u16}"""
    sub, items = _range_items(p)
    for it in items:
        e = it.pop('extra')
        if len(e) == 8:
            it['extra'] = list(struct.unpack_from('<4H', e, 0))
        elif e:
            it['extra'] = e.hex()
    return dict(type='cuts', pool_size=sub, items=items)


def ch_points(p):
    """type 3: u16 count, u16 subtype, items {u16 size, u16 time, payload(u32...)}"""
    count, sub = struct.unpack_from('<HH', p, 0)
    items, pos = [], 4
    for _ in range(count):
        size, t = struct.unpack_from('<HH', p, pos)
        words = list(struct.unpack_from('<%dI' % ((size - 4) // 4), p, pos + 4))
        items.append(dict(t=t, words=words, hex=[f'{w:08x}' for w in words]))
        pos += size
    return dict(type='points', pool_size=sub, items=items)


CH_DECODERS = {0: ch_curve, 1: ch_range, 2: ch_cuts, 3: ch_points}


# ----------------------------------------------------------------------------------------------
# objects

def ext_transform(e, off):
    v = struct.unpack_from('<6f', e, off)
    return dict(translate=[f32(x) for x in v[:3]], rotate=[f32(x) for x in v[3:]])


def anchor_name(a):
    """Anchor ids (0x27A0D8 / jump table 0x481d00)."""
    if a == 0 or a > 60: return 'world origin'
    if a <= 18: return f'start-grid slot of subject {a + 2}'
    return {19: 'locator 7 (ground-snapped)', 20: 'locator 0 + (-20,0,340) (podium 1st step)',
            21: 'locator 0 + (-20,-275,280) (podium 2nd step)', 22: 'locator 0 + (-20,275,280) (podium 3rd step)',
            23: 'locator 0 (ground-snapped)', 24: 'locator 8', 25: 'locator 1 = start gate, z-1000 then ground-snapped',
            26: 'locator 2 (ground-snapped)', 27: 'locator 3 (ground-snapped)', 28: 'locator 4 (ground-snapped)',
            29: 'locator 5 (set 0x2B if loop flag)', 30: 'locator 6 (set 0x2B if loop flag)', 31: 'locator 9 (ground-snapped)',
            32: 'locator 10', 33: 'locator 11', 34: 'locator 12 (ground-snapped)', 35: 'locator 13 (ground-snapped)',
            36: 'locator 14 (ground-snapped)', 37: 'locator 15', 38: 'locator 16', 39: 'locator 17'}.get(a, f'live actor {a - 40}')


def binding_name(b):
    """Actor binding id = subject id + 1 (0x280458 -> 0x279F18(mgr, id-1))."""
    if b == 0: return 'any (first rider handler)'
    if b <= 3: return f'script participant {b - 1} (podium place {b} / rival)'
    if b <= 5: return f'human player {b - 3}'
    if b <= 10: return f'AI rider list A[{b - 6}] (C+0x48, falls back to C+0x5C)'
    if b <= 15: return f'opponent list B[{b - 11}] (C+0x5C, falls back to C+0x48 AI)'
    if b <= 21: return f'race rider index {b - 16}'
    return 'never matches'


def subject_name(s):
    """Subject ids (0x279F18)."""
    if s < 0: return None
    if s <= 2: return f'script participant {s}'
    if s <= 4: return f'human player {s - 2}'
    if s <= 9: return f'AI/remote list A[{s - 5}]'
    if s <= 14: return f'AI/remote list B[{s - 10}]'
    if s <= 20: return f'rider[{s - 15}]'
    return f'subject {s}'


def decode_ext(kind, e):
    if kind in (0, 1, 2, 3) and len(e) >= 0x24:
        near, far = struct.unpack_from('<2f', e, 0)
        o = struct.unpack_from('<6f', e, 8)
        d = dict(near=f32(near), far=f32(far),
                 offset=dict(translate=[f32(x) for x in o[:3]], yaw=f32(o[3]), pitch=f32(o[4]), roll=f32(o[5])),
                 anchor=e[0x20], anchor_name=anchor_name(e[0x20]), camera_id=e[0x21])
        if kind == 2:
            d['subject'] = struct.unpack_from('<b', e, 0x22)[0]
            d['subject_name'] = subject_name(d['subject'])
        d['pad'] = e[0x22:].hex()
        return d
    if kind == 3 and len(e) >= 0x1f:
        near, far, yaw, pitch, roll, lerp, vmax = struct.unpack_from('<7f', e, 0)
        return dict(near=f32(near), far=f32(far), yaw=f32(yaw), pitch=f32(pitch), roll=f32(roll), lerp=f32(lerp),
                    max_speed=f32(vmax), camera_id=e[0x1c], subject=e[0x1d], absolute=e[0x1e])
    if kind == 5 and len(e) >= 0x20:
        bind, anc = struct.unpack_from('<bb', e, 0)
        cmask = struct.unpack_from('<H', e, 2)[0]
        o = struct.unpack_from('<6f', e, 8)
        return dict(binding=bind, binding_name=binding_name(bind), anchor=anc, anchor_name=anchor_name(anc),
                    character_mask=f'{cmask:#05x}',
                    characters='any' if cmask in (0, 0x3ff) else [CHARDB[c] for c in range(10) if cmask >> c & 1],
                    flag_skip_visibility=e[4], exit_mode=e[5], hide_flag40_items=e[6], root_motion_velocity=e[7],
                    offset=dict(translate=[f32(x) for x in o[:3]], yaw=f32(o[3]), rot_y=f32(o[4]), rot_x=f32(o[5])))
    if kind == 4 and len(e) >= 4:
        return dict(b0=e[0], b1=e[1], b2=e[2], b3=e[3])
    return dict(raw=e.hex())


KNOWN_CLIPS = {f'{game_hash(n.encode()):08x}': [n] for n in ('heli_idle1', 'gond_idle1', 'gond_idle2')}
NOSCRIPT = game_hash(b'NoScript')  # 0x049A9AD4
STAGE_SYMS = {0x0e995385: 'heli landing (stop/restart TRANSPORT loop 201, LiveComp clip on helicopter)',
              0x0ae69ab4: 'heli idle (stop/restart TRANSPORT loop 201, LiveComp clip on helicopter)',
              0x03e0ea1e: 'rotor-wash particles on', 0x03efc174: 'rotor-wash particles off'}


def annotate(obj):
    """Attach engine semantics to decoded channel items (see docs/cutscenes.md)."""
    k, ch = obj['kind'], obj['channels']
    if k == 7:
        for it in ch[0]['items']:
            call, clean = (it['words'] + [0, 0])[:2]
            it['call'] = f'{call:08x}'; it['call_desc'] = STAGE_SYMS.get(call)
            it['cleanup'] = None if clean == NOSCRIPT else f'{clean:08x}'
            it['cleanup_desc'] = STAGE_SYMS.get(clean)
        for it in ch[1]['items']:
            it['music_code'] = it['words'][0]; it['desc'] = MUSIC_CODES.get(it['words'][0], 'ignored' if it['words'][0] == 0xff else None)
        for it in ch[2]['items']:
            it['cue'] = it['words'][0]; it['desc'] = PA_CUES.get(it['words'][0])
        for it in ch[3]['items']:
            it['voice_slot'] = it['index']; it['sound_index'] = it['value']
            it['volume'] = min(127, int(it['tag'][:2], 16)) if it.get('tag') else None
        for it in ch[4]['items']:
            it['desc'] = 'weather/snow off (gp 0x4A3B60 = 1) while active'
    elif k == 5:
        for it in ch[0]['items']:
            it['clip_index'] = it['value']
        for it in ch[1]['items']:
            it['voice_slot'] = it['index']; it['sound_index'] = it['value']
            it['volume'] = min(127, it['tail'][0]) if it.get('tail') else None
        for it in ch[2]['items']:
            ev = it['words'][0] if it['words'] else None
            it['event'] = ev
            it['desc'] = {2: 'speech Finish_Line_<char> (0x2A1400)', 100: 'speech BC_Challenge_<char> (0x2A1138)',
                          101: 'speech Hey_<char> (0x2A1280)', 0x66: 'ignored'}.get(ev, 'no-op')
            if len(it['words']) > 1:
                tgt = struct.unpack('<b', struct.pack('<I', it['words'][1])[:1])[0]
                it['target_subject'] = tgt; it['target_name'] = subject_name(tgt)
    elif k == 4:
        for it in ch[0]['items']:
            it['camera_id'] = it['value']; it['transition_ticks'] = it['value2']
            ex = it.get('extra')
            if isinstance(ex, list):
                raw = struct.pack('<4H', *ex)
                tt, p1, p2, p3 = struct.unpack_from('<bbhh', raw, 0)
                it['transition'] = {0: 'blend', 7: 'fade', 8: 'cut'}.get(tt, f'screen transition {tt}')
                it['transition_params'] = [p1, p2, p3]
                if it['value2'] == 0 and tt == 0:
                    it['transition'] = 'cut'
    return obj


def decode_object(ob, fileoff):
    hs, nch, kind = struct.unpack_from('<HBB', ob, 0)
    dur = struct.unpack_from('<I', ob, 4)[0] if hs >= 8 else None
    ext = ob[8:hs] if hs >= 9 else b''
    labels = CHANNEL_LABELS.get(kind, [])
    chans, p = [], hs
    for i in range(nch):
        t, sz = struct.unpack_from('<HH', ob, p)
        c = CH_DECODERS[t](ob[p + 4:p + 4 + sz])
        c['index'] = i
        c['label'] = labels[i] if i < len(labels) else f'ch{i}'
        chans.append(c); p += 4 + sz
    assert p == len(ob)
    return annotate(dict(offset=fileoff, kind=kind, kind_name=KIND_NAMES.get(kind, f'kind{kind}'), header_size=hs,
                         duration=dur, ext=decode_ext(kind, ext), ext_hex=ext.hex(), channels=chans))


def decode_isb(b):
    ver, ntr, hs = struct.unpack_from('<BBH', b, 0)
    magic, size = struct.unpack_from('<II', b, 4)
    if magic != MAGIC or size != len(b):
        raise ValueError('bad isb')
    mask = struct.unpack_from('<I', b, 0x10)[0]
    fades = []
    for o in (0x14, 0x1c):
        ft, col, a, bb, c = struct.unpack_from('<bbhhh', b, o)
        fades.append(dict(type=ft, type_name=FADE_TYPES.get(ft, f'type{ft}'), colour='white' if col == 1 else 'black',
                          out_ticks=a, hold_ticks=bb, in_ticks=c))
    hdr = dict(version=ver, track_count=ntr, header_size=hs, size=size, loop=b[0x0c], b0d_unused=b[0x0d:0x10].hex(),
               location_mask=f'{mask:#08x}', events=[n for i, n in enumerate(LOC_BITS) if mask >> i & 1],
               fade_in=fades[0], fade_out=fades[1], script_number=struct.unpack_from('<I', b, 0x24)[0],
               load_group_key=struct.unpack_from('<I', b, 0x28)[0])
    tracks, p = [], hs
    for ti in range(ntr):
        tsz, cnt = struct.unpack_from('<II', b, p)
        offs = struct.unpack_from('<%dI' % cnt, b, p + 8)
        alts = []
        for j, o in enumerate(offs):
            end = offs[j + 1] if j + 1 < cnt else tsz
            alts.append(decode_object(b[p + o:p + end], p + o))
        tracks.append(dict(index=ti, offset=p, size=tsz, alternatives=alts))
        p += tsz
    assert p == len(b)
    return hdr, tracks


# ----------------------------------------------------------------------------------------------
# banks

def anim_bank_summary(data, names):
    if not data:
        return None
    bank = read_bank(data)
    clips = []
    for i, a in enumerate(bank['animations']):
        parts = sorted({c['part'] for c in bank['channels'][a['first_channel']:a['first_channel'] + a['part_count'] * a['segment_count']]})
        clips.append(dict(index=i, hash=a['hash'], name=sorted(names.get(a['hash'], [])) or KNOWN_CLIPS.get(a['hash']),
                          frames=a['frame_count_field'], part_count=a['part_count'], parts=parts,
                          events=a['events']))
    return dict(size=len(data), clips=clips)


def sound_bank_summary(data):
    if not data:
        return None
    b = parse_bank(data)
    out = []
    for e in b['entries']:
        if e is None:
            out.append(None); continue
        p = e['patches'][0]
        out.append(dict(index=e['index'], patches=len(e['patches']), rate=p['sampleRate'], samples=p['sampleCount'],
                        seconds=round(p['sampleCount'] / p['sampleRate'], 3) if p['sampleRate'] else None,
                        codec=p['codecName']))
    return dict(size=len(data), count=b['count'], sounds=out)


def string_hash_names():
    import re
    names = {}
    for p in (REPO / 'local/disc/SLUS_207.72',):
        for raw in re.findall(rb'[\x20-\x7e]{3,}', p.read_bytes()):
            for v in (raw, raw.lower(), raw.upper()):
                names.setdefault(f'{game_hash(v):08x}', set()).add(v.decode())
    extra = REPO / 'local/assets/animation/name-matches.json'
    if extra.exists():
        try:
            j = json.loads(extra.read_text())
            if isinstance(j, dict):
                for k, v in j.items():
                    try:
                        key = f'{int(k, 16):08x}'
                    except ValueError:
                        continue
                    if isinstance(v, list):
                        names.setdefault(key, set()).update(x for x in v if isinstance(x, str))
                    elif isinstance(v, str):
                        names.setdefault(key, set()).add(v)
        except Exception:
            pass
    return names


# ----------------------------------------------------------------------------------------------
# summary helpers

def script_summary(tracks):
    kinds = {}
    for t in tracks:
        k = t['alternatives'][0]['kind']; kinds.setdefault(KIND_NAMES.get(k, k), 0); kinds[KIND_NAMES.get(k, k)] += 1
    dur_alt = [[a['duration'] for a in t['alternatives']] for t in tracks]
    clips, sounds = set(), set()
    for t in tracks:
        for a in t['alternatives']:
            if a['kind'] == 5:
                for it in a['channels'][0]['items']:
                    clips.add(it['value'])
    return dict(kinds=kinds, actors=kinds.get('actor', 0),
                duration_ticks=max(max(d) for d in dur_alt), duration_seconds=round(max(max(d) for d in dur_alt) / TICK_HZ, 3),
                alternatives=[len(t['alternatives']) for t in tracks], clip_indices=sorted(clips))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    from disc_paths import ps2_iso;ap.add_argument('--iso', default=str(ps2_iso()))
    ap.add_argument('--out', default=str(REPO / 'local/cutscenes/isb'))
    args = ap.parse_args()
    out = Path(args.out); out.mkdir(parents=True, exist_ok=True)
    m = load_scdat(args.iso)
    master = decode_master(m['scmaster.dat'], m['scmasterdbg.dat'])
    names = string_hash_names()
    # containers: every .big member of SCDAT.BIG
    containers = {}
    for fname, data in m.items():
        if fname.endswith('.big'):
            ents = big_entries(data)
            containers[fname[:-4]] = {n.split('\\')[-1].split('/')[-1]: d for n, d in ents}
    by_script = {}
    for cname, ents in containers.items():
        anm = next((d for n, d in ents.items() if n.startswith('anm')), None)
        anm_name = next((n for n in ents if n.startswith('anm')), None)
        snd = next((d for n, d in ents.items() if n.startswith('snd')), None)
        snd_name = next((n for n in ents if n.startswith('snd')), None)
        for n, d in ents.items():
            if n.startswith('scr') and n.endswith('.isb'):
                num = int(n[3:11])
                by_script.setdefault(num, []).append(dict(container=cname + '.big', isb=d, anm_name=anm_name, anm=anm,
                                                          snd_name=snd_name, snd=snd))
    index = []
    bank_cache = {}

    def bank(kind, name, data):
        key = (kind, name, len(data) if data else 0, hash(data))
        if key not in bank_cache:
            bank_cache[key] = anim_bank_summary(data, names) if kind == 'anm' else sound_bank_summary(data)
        return bank_cache[key]

    for num in sorted(by_script):
        copies = by_script[num]
        primary = next((c for c in copies if c['container'] == f'{num:08d}.big'), copies[0])
        hdr, tracks = decode_isb(primary['isb'])
        rec = dict(number=num, name=master[num]['name'] if num < len(master) else None,
                   hash=master[num]['hash'] if num < len(master) else None,
                   tick_hz=TICK_HZ, container=primary['container'], header=hdr,
                   summary=script_summary(tracks), tracks=tracks,
                   anim_bank=dict(file=primary['anm_name'], **(bank('anm', primary['anm_name'], primary['anm']) or {})) if primary['anm'] else None,
                   sound_bank=dict(file=primary['snd_name'], **(bank('snd', primary['snd_name'], primary['snd']) or {})) if primary['snd'] else None,
                   other_copies=[])
        for c in copies:
            if c is primary:
                continue
            same = c['isb'] == primary['isb']
            oc = dict(container=c['container'], identical=same, anim_bank=c['anm_name'], sound_bank=c['snd_name'])
            if not same:
                h2, t2 = decode_isb(c['isb'])
                oc['tracks'] = t2
                oc['summary'] = script_summary(t2)
                oc['anim_bank_clips'] = (bank('anm', c['anm_name'], c['anm']) or {}).get('clips') if c['anm'] else None
            rec['other_copies'].append(oc)
        (out / f'{num:03d}.json').write_text(json.dumps(rec, indent=1))
        index.append(dict(number=num, name=rec['name'], container=rec['container'], size=hdr['size'],
                          loop=hdr['loop'], events=hdr['events'], **rec['summary'],
                          anm=primary['anm_name'], anm_clips=len(rec['anim_bank']['clips']) if rec['anim_bank'] else 0,
                          snd=primary['snd_name'], snd_count=rec['sound_bank']['count'] if rec['sound_bank'] else 0,
                          copies=[c['container'] for c in copies]))
    (out / 'index.json').write_text(json.dumps(dict(master=master, scripts=index), indent=1))
    filt = {n: decode_filter(d, master) for n, d in m.items() if n.startswith('scfilter')}
    (out / 'scfilter.json').write_text(json.dumps(filt, indent=1))
    print(f'wrote {len(index)} scripts to {out}')


if __name__ == '__main__':
    main()
