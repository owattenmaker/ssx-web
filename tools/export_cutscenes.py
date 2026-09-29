#!/usr/bin/env python3
"""Export the original SSX 3 in-engine cutscenes (NIS scripts) for the browser (read-only from the ISO).

Source: DATA/SCRIPTS/SCDAT.BIG (docs/cutscenes.md). Every script container is a BIGF holding
  scrNNNNNNNN.isb   the script (decoded by tools/cutscene_isb.py)
  anmNNNNNNNN.afl   its animation bank (AFL, the ANM.BIG format; decoded like tools/export_animation_library.py)
  sndNNNNNNNN.bnk   its sound bank (BNKl v5; tools/export_audio.py parse_bank, played by web/sfx.js)
Location containers (scdat_<LOC>.big, scdat_main.big) hold several scripts plus one shared bank.

Writes (git-ignored) web/public/assets/CUTSCENES/:
  index.json              every script (number, name, containers, duration), containers, scfilter choice lists
  scripts/<container>.NNN.json  runtime form of each script copy (tools/cutscene_isb.py decode)
  locators.json           anchor locators per location + start grids (tools/cutscene_locators.py)
  anim/<container>.json   clips of the container's AFL in the web/public/assets/ANIMATIONS/library.json shape
  anim/<container>.f32    their samples (little-endian float32, authored 30 Hz)
  banks/<container>.json + .bnk  the container's sound bank (web/sfx.js loadBank('../../CUTSCENES/banks/<container>'))
No motion is invented: clips are sampled from the original curves only.

  python3 tools/export_cutscenes.py [--iso PATH] [--out DIR]
"""
import argparse, array, hashlib, json, re, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_disc import Disc  # noqa: E402
from export_audio import BigOnDisc, big_entries, parse_bank  # noqa: E402
from animation_bank import read_bank, game_hash  # noqa: E402
from animation_curves import CurvePacket  # noqa: E402

from disc_paths import ps2_iso;ISO = ps2_iso()
OUT = ROOT / 'web/public/assets/CUTSCENES'
SCDAT = 'DATA/SCRIPTS/SCDAT.BIG'


def containers(disc):
    """Yield (container name, {member name: bytes}) for every script container of SCDAT.BIG."""
    big = BigOnDisc(disc, SCDAT)
    for e in big.entries:
        name = e['name'].split('/')[-1]
        if not name.endswith('.big') or not e['size']:
            continue
        data = disc.read(big.base + e['offset'], e['size'])
        yield name[:-4], {m['name']: data[m['offset']:m['offset'] + m['size']] for m in big_entries(data)}


def master(disc):
    """scmaster.dat: u32 count, then {u32 hash, u32 number, u32 flags} per script (tools/cutscene_isb.py master)."""
    big = BigOnDisc(disc, SCDAT)
    e = next(x for x in big.entries if x['name'].endswith('scmaster.dat'))
    return disc.read(big.base + e['offset'], e['size'])


def decode_clips(raw, bank_name):
    """AFL bank -> (clips, payload) in the ANIMATIONS/library.json shape (per-part streams, 30 Hz)."""
    bank = read_bank(raw)
    payload = bytearray(); clips = []
    for index, animation in enumerate(bank['animations']):
        parts = {}
        first = animation['first_channel']
        for channel in bank['channels'][first:first + animation['part_count'] * animation['segment_count']]:
            at = bank['payload_offset'] + channel['offset']; data = bytearray(raw[at:at + channel['byte_size']])
            packet = CurvePacket(data, channel['frame_field'])
            for word in range(packet.offsets[8], packet.offsets[8] + packet.sizes[8], 2):
                data[word:word + 2] = data[word:word + 2][::-1]
            packet = CurvePacket(data, channel['frame_field']); frames = parts.setdefault(str(channel['part']), [])
            frames.extend(packet.sample(frame) for frame in range(1 if frames else 0, packet.frames))
        streams = {}
        for part, frames in parts.items():
            count = len(frames[0])
            if any(len(f) != count for f in frames):
                raise ValueError(f'{bank_name}/{index}: changing channel layout')
            values = array.array('f', (v for f in frames for v in f))
            if sys.byteorder != 'little':
                values.byteswap()
            streams[part] = dict(offset=len(payload), channels=count, frames=len(frames))
            payload.extend(values.tobytes())
        clips.append(dict(id=f'{bank_name}:{index}', index=index, name=f'{bank_name.upper()}_{animation["hash"]}',
                          bank=bank_name, source_hash=animation['hash'], fps=30,
                          frame_count=animation['frame_count_field'], duration=(animation['frame_count_field'] - 1) / 30,
                          loop=False, events=animation['events'], streams=streams))
    return clips, bytes(payload)


# ------------------------------------------------------------------------------------------------
# runtime script form (web/cutscenes.js): compact, exact float32 coefficients kept

def _curve(c):
    flat = []
    for k in c['keys']:
        flat.append(k['t']); flat.extend(k['abcd'])
    return dict(t='c', k=flat)


def _ranges(c):
    return dict(t='r', i=[dict(s=it['index'], t0=it['t0'], t1=it['t1'], v=it['value'], o=it.get('start_offset', 0),
                               sp=it.get('speed', 1.0), b=it.get('blend_ticks', 0), tail=it.get('tail', [0, 0, 0, 0]))
                          for it in c['items']])


def _cuts(c):
    out = []
    for it in c['items']:
        ex = it.get('extra'); kind = 8
        if isinstance(ex, list):
            kind = struct.unpack_from('<b', struct.pack('<H', ex[0]))[0]
        out.append(dict(s=it['index'], t0=it['t0'], t1=it['t1'], cam=it['value'], d=it['value2'], x=kind))
    return dict(t='x', i=out)


def _points(c):
    return dict(t='p', i=[dict(t=it['t'], w=it['words']) for it in c['items']])


def runtime_object(ob):
    ch = []
    for c in ob['channels']:
        ch.append({'curve': _curve, 'ranges': _ranges, 'cuts': _cuts, 'points': _points}[c['type']](c))
    ext = {k: v for k, v in ob['ext'].items() if not k.endswith('_name') and k not in ('pad', 'raw', 'characters')}
    return dict(kind=ob['kind'], dur=ob['duration'], ext=ext, ch=ch)


def runtime_script(number, name, container, hdr, tracks):
    return dict(number=number, name=name, container=container, loop=hdr['loop'],
                fade_in=hdr['fade_in'], fade_out=hdr['fade_out'], locations=hdr['events'],
                duration=max(a['duration'] or 0 for t in tracks for a in t['alternatives']),
                tracks=[[runtime_object(a) for a in t['alternatives']] for t in tracks])


def main():
    import cutscene_isb as isb
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--iso', type=Path, default=ISO)
    ap.add_argument('--out', type=Path, default=OUT)
    args = ap.parse_args()
    out = args.out
    for sub in ('scripts', 'anim', 'banks'):
        (out / sub).mkdir(parents=True, exist_ok=True)
    disc = Disc(args.iso)
    try:
        big = BigOnDisc(disc, SCDAT)
        raw = {e['name'].split('/')[-1]: disc.read(big.base + e['offset'], e['size']) for e in big.entries if e['size']}
    finally:
        disc.close()
    master = isb.decode_master(raw['scmaster.dat'], raw['scmasterdbg.dat'])
    scripts = {}; containers = {}
    for fname, data in sorted(raw.items()):
        if not fname.endswith('.big'):
            continue
        cname = fname[:-4]; members = {n.split('/')[-1]: data[m['offset']:m['offset'] + m['size']]
                                       for m in big_entries(data) for n in [m['name']]}
        info = dict(name=cname, scripts=[], anim=None, bank=None)
        for n, d in members.items():
            if n.startswith('anm') and d:
                clips, payload = decode_clips(d, cname)
                (out / 'anim' / f'{cname}.f32').write_bytes(payload)
                (out / 'anim' / f'{cname}.json').write_text(json.dumps(dict(
                    version=1, encoding='little-endian float32', source=f'{SCDAT}|{fname}|{n}',
                    sha256=hashlib.sha256(d).hexdigest(), byte_length=len(payload), clips=clips), separators=(',', ':')))
                info['anim'] = dict(file=f'anim/{cname}.json', clips=len(clips))
            elif n.startswith('snd') and d[:4] == b'BNKl':
                (out / 'banks' / f'{cname}.bnk').write_bytes(d)
                bank = parse_bank(d)
                (out / 'banks' / f'{cname}.json').write_text(json.dumps(dict(file=f'../../CUTSCENES/banks/{cname}.bnk', original=f'{fname}|{n}',
                                                                             source='SCDAT', **bank), separators=(',', ':')))
                info['bank'] = dict(name=cname, sounds=sum(1 for e in bank['entries'] if e))
        for n, d in members.items():
            if n.startswith('scr') and n.endswith('.isb'):
                number = int(n[3:11]); hdr, tracks = isb.decode_isb(d)
                rt = runtime_script(number, master[number]['name'], cname, hdr, tracks)
                (out / 'scripts' / f'{cname}.{number:03d}.json').write_text(json.dumps(rt, separators=(',', ':')))
                info['scripts'].append(number)
                scripts.setdefault(number, dict(number=number, name=master[number]['name'], containers=[],
                                                duration=rt['duration'], loop=rt['loop']))['containers'].append(cname)
        containers[cname] = info
    filters = {}
    for fname, data in raw.items():
        if fname.startswith('scfilter') and fname.endswith('.dat') and fname != 'scfilterdbg.dat':
            loc = fname[8:-4]; f = isb.decode_filter(data, master)
            n = struct.unpack_from('<I', data, 0)[0]; lists = []
            for i in range(n):
                cnt, off = struct.unpack_from('<II', data, 4 + 8 * i)
                lists.append([dict(script=struct.unpack_from('<I', data, off + 0x30 * j)[0],
                                   masks=list(struct.unpack_from('<21H', data, off + 0x30 * j + 4))) for j in range(cnt)])
            filters[loc] = lists
    index = dict(version=1, source=SCDAT, tick_hz=60, docs='docs/cutscenes.md',
                 scripts=[scripts[k] for k in sorted(scripts)], containers=containers, filters=filters,
                 rider_bits=['moby', 'kaori', 'allegra', 'mac', 'zoe', 'griff', 'elise', 'nate', 'psymon', 'viggo'])
    (out / 'index.json').write_text(json.dumps(index, separators=(',', ':')))
    # anchor locators (world record kind 18) + AIP start grids, tools/cutscene_locators.py
    import cutscene_locators
    loc = cutscene_locators.decode(); slim = {}
    for name, e in loc['locations'].items():
        ls = {t: {k: v[k] for k in ('pos', 'yaw', 'pitch', 'instance_name', 'podium_steps', 'anchor25_before_snap') if k in v}
              for t, v in e['locators'].items() if 'pos' in v}
        if ls or e.get('start_grid'):
            slim[name] = dict(locators=ls, start_grid=[{k: g[k] for k in ('slot', 'pos', 'yaw', 'pitch')} for g in e.get('start_grid', [])])
    (out / 'locators.json').write_text(json.dumps(dict(units='PS2 world cm, z up, radians', source='BAM.BIG kind-18 records + AIP start grid',
                                                        anchors=loc['anchor_ids'], locations=slim), separators=(',', ':')))
    print(f'Exported {len(scripts)} cutscene scripts, {len(containers)} containers, {len(filters)} filters: {out}')


if __name__ == '__main__':
    main()
