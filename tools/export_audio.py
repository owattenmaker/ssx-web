#!/usr/bin/env python3
"""Export SSX 3 audio for the browser in its original compressed form (read-only).

Reads the user's ISO and writes to web/public/assets/AUDIO/ (git-ignored):

  catalog.json                 songs (MUSIC.INF: titles, artists, BPM, levels, ...), the default
                               playlist (PLAYLIST.INF), mixer presets (MIX.INF), BANKS.INF,
                               CROWD.INF, and the list of exported music/speech/bank files
  music/<song>.json            Pathfinder (.mpf v4) graph: nodes, branches, events, tracks and
                               every sample (segment) with its byte range / bank slice
  music/<file>.mus             original stream file (EA SCxl streams, EA-XA) and loop bank (BNKl)
  speech/<name>.json + .dat    line index from the .hdr tables + the original .dat (SCxl streams)
  banks/<name>.json + .bnk     BNKl v5 sound banks (AUDIO.BIG, GRNT_*.BNK) with every patch
  banks/<name>.eam             crowd MIDI (MIDx), copied as-is

No PCM is written: the payloads stay EA-XA / PS-ADPCM / PCM8 / MicroTalk and are decoded in the
browser by web/audio-decode.js. The formats are documented in docs/audio-formats.md.

  python3 tools/export_audio.py                       # everything (~1.4 GB, mostly speech+music)
  python3 tools/export_audio.py --only music --songs go,deep
  python3 tools/export_audio.py --only speech,banks
"""
import argparse, json, re, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_disc import Disc  # noqa: E402

from disc_paths import ps2_iso;ISO = ps2_iso()
OUT = ROOT / 'web/public/assets/AUDIO'
CODEC_NAMES = {4: 'microtalk', 5: 'ps-adpcm', 7: 'pcm16be', 8: 'pcm16le', 9: 'pcm8', 10: 'ea-xa'}
# Defaults the EE applies when a PT tag is absent (0x3BB820 streams, 0x3BAAC0 bank patches).
STREAM_DEFAULTS = dict(sampleRate=22050, channels=1, codec=5)
BANK_DEFAULTS = dict(sampleRate=22050, channels=1, codec=5)


# ------------------------------------------------------------------------------------------------
# containers

def big_entries(data):
    """BIGF archive: 'BIGF', u32 size, u32 BE count, u32 BE header size, {u32 BE offset, u32 BE size, name\\0}*."""
    if data[:4] != b'BIGF':
        raise ValueError('not a BIGF archive')
    count, _ = struct.unpack_from('>II', data, 8)
    pos, out = 16, []
    for _ in range(count):
        off, size = struct.unpack_from('>II', data, pos); pos += 8
        end = data.index(b'\0', pos); name = data[pos:end].decode('latin1'); pos = end + 1
        out.append(dict(name=name, offset=off, size=size))
    return out


class BigOnDisc:
    """BIGF archive read lazily from the ISO (only the directory is loaded)."""
    def __init__(self, disc, path):
        e = next(x for x in disc.entries if x['path'].upper() == path.upper())
        self.disc, self.base, self.path = disc, e['offset'], path
        head = disc.read(self.base, 16)
        hsize = struct.unpack_from('>I', head, 12)[0]
        self.entries = big_entries(disc.read(self.base, hsize + 16))
        self.by_name = {x['name'].split('\\')[-1].lower(): x for x in self.entries}

    def read(self, name):
        e = self.by_name[name.lower()]
        return self.disc.read(self.base + e['offset'], e['size']) if e['size'] else b''

    def names(self, ext):
        return [x['name'].split('\\')[-1] for x in self.entries if x['name'].lower().endswith(ext) and x['size']]


def parse_pt(b, pos):
    """PT header (EE 0x3C7388). Returns (platform, [ {params:{}, sample:{}} ... ], end)."""
    if b[pos:pos + 2] != b'PT':
        raise ValueError(f'PT header expected at {pos:#x}')
    platform = b[pos + 2]; p = pos + 4
    patches, cur, in_sample = [], dict(params={}, sample={}), False
    while True:
        tag = b[p]; p += 1
        if tag == 0xFC: continue
        if tag == 0xFF: break
        if tag == 0xFD: in_sample = True; continue
        if tag == 0xFE: patches.append(cur); cur = dict(params={}, sample={}); in_sample = False; continue
        ln = b[p]; p += 1
        if ln == 0xFF: ln = struct.unpack_from('>I', b, p)[0]; p += 4
        raw = b[p:p + ln]; p += ln
        val = int.from_bytes(raw, 'big') if ln <= 4 else raw.hex()
        (cur['sample'] if (in_sample or tag >= 0x80) else cur['params'])[tag] = val
    patches.append(cur)
    return platform, patches, p


def sample_info(sample, defaults):
    offsets = [sample[t] for t in (0x88, 0x89, 0x8B) if t in sample]
    codec = sample.get(0xA0, defaults['codec'])
    return dict(version=sample.get(0x80, 0), codec=codec, codecName=CODEC_NAMES.get(codec, f'codec{codec}'),
                channels=sample.get(0x82, defaults['channels']), sampleRate=sample.get(0x84, defaults['sampleRate']),
                sampleCount=sample.get(0x85, 0), loopStart=sample.get(0x86, -1), loopEnd=sample.get(0x87, -1),
                dataOffsets=offsets)


def hexkeys(d):
    return {f'{k:#04x}': v for k, v in sorted(d.items())}


def walk_stream(b, pos):
    """One SCHl..SCEl stream starting at pos. Returns header info + end offset."""
    if b[pos:pos + 4] != b'SCHl':
        raise ValueError(f'SCHl expected at {pos:#x}')
    hsize = struct.unpack_from('<I', b, pos + 4)[0]
    platform, patches, _ = parse_pt(b, pos + 8)
    info = sample_info(patches[0]['sample'], STREAM_DEFAULTS)
    info['platform'] = platform
    info['params'] = hexkeys(patches[0]['params'])
    extra = {k: v for k, v in patches[0]['sample'].items() if k not in (0x80, 0x82, 0x84, 0x85, 0x86, 0x87, 0xA0)}
    if extra: info['tags'] = hexkeys(extra)
    p = pos + hsize; blocks = decoded = 0; loop = None
    while True:
        tid = b[p:p + 4]; size = struct.unpack_from('<I', b, p + 4)[0]
        if size < 8: raise ValueError(f'bad block size at {p:#x}')
        if tid == b'SCDl': blocks += 1; decoded += struct.unpack_from('<I', b, p + 8)[0]
        elif tid == b'SCLl': loop = struct.unpack_from('<I', b, p + 8)[0]
        elif tid == b'SCEl': p += size; break
        elif tid != b'SCCl': raise ValueError(f'unexpected block {tid!r} at {p:#x}')
        p += size
    info.update(blocks=blocks, decodedSamples=decoded, end=p)
    if loop is not None: info['loopBlock'] = loop
    return info


def all_streams(b):
    """Every SCxl stream in a concatenated .dat/.mus (streams are padded with zero bytes)."""
    pos, out = 0, []
    while True:
        pos = b.find(b'SCHl', pos)
        if pos < 0: return out
        info = walk_stream(b, pos); info['offset'] = pos; out.append(info); pos = info['end']


def parse_bank(b):
    """BNKl v5 (see docs/audio-formats.md)."""
    if b[:4] != b'BNKl':
        raise ValueError('not a BNKl bank')
    count = struct.unpack_from('<H', b, 6)[0]
    header_size, spu_size, ram_size = struct.unpack_from('<III', b, 8)
    entries = []
    for i in range(count):
        slot = 0x14 + 4 * i; rel = struct.unpack_from('<I', b, slot)[0]
        if not rel: entries.append(None); continue
        platform, patches, end = parse_pt(b, slot + rel)
        out = []
        for p in patches:
            info = sample_info(p['sample'], BANK_DEFAULTS)
            info['params'] = hexkeys(p['params'])
            extra = {k: v for k, v in p['sample'].items() if k not in (0x80, 0x82, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8B, 0xA0)}
            if extra: info['tags'] = hexkeys(extra)
            out.append(info)
        entries.append(dict(index=i, headerOffset=slot + rel, patches=out))
    return dict(version=b[4], flags=b[5], count=count, headerSize=header_size, spuDataSize=spu_size,
                ramDataSize=ram_size, size=len(b), entries=entries)


# ------------------------------------------------------------------------------------------------
# INF files

def parse_inf(text):
    """[Section] blocks of KEY = value lines. Repeated keys become lists; '#' starts a comment."""
    sections, cur = [], None
    for raw in text.splitlines():
        line = raw.split('#', 1)[0].strip()
        if not line: continue
        m = re.match(r'^\[(.+?)\]\s*$', line)
        if m:
            cur = dict(name=m.group(1).strip(), values=[]); sections.append(cur); continue
        m = re.match(r'^([A-Za-z0-9_]+)\s*=\s*(.*)$', line)
        if m and cur is not None:
            v = m.group(2).strip()
            if v.startswith('"'): v = v[1:v.rindex('"')] if v.count('"') >= 2 else v[1:]
            else:
                try: v = float(v) if '.' in v else int(v)
                except ValueError: pass
            cur['values'].append((m.group(1), v))
    return sections


MUSIC_KEYS = ['ADDTOFE', 'TITLE', 'ARTIST', 'ALBUM', 'PATHDATA', 'MUSDATA', 'LOOPDATA', 'BPM', 'BeatsPerMeasure',
              'MeasuresPerBar', 'PhrasesPerBank', 'BeatsPerPhrase', 'PhraseAlign', 'DelayCount', 'DelayTime',
              'DelayFeedback', 'DelayLevel', 'PathLevel', 'AsyncLevel', 'CATEGORY', 'DUCKTOLOOPS', 'SEDVALUE',
              'LOWPASS', 'SONGBIG', 'PREVIEW']
MUSIC_DEFAULTS = dict(BPM=120.0, BeatsPerMeasure=4, MeasuresPerBar=2, PhrasesPerBank=4, BeatsPerPhrase=8,
                      PhraseAlign=16, DelayCount=0, DelayTime=100, DelayFeedback=90, DelayLevel=50, PathLevel=100,
                      AsyncLevel=100, DUCKTOLOOPS=1, SEDVALUE=-1, SONGBIG=1)
CATEGORY_NAMES = {0: 'Race', 1: 'SlopeStyle', 2: 'BigAir', 3: 'HalfPipe', 4: 'BackCountry'}


def music_songs(text):
    canon = {k.lower(): k for k in MUSIC_KEYS}
    songs = []
    for sec in parse_inf(text):
        if sec['name'].upper() == 'GLOBAL': continue
        s = dict(id=sec['name'], categories=[], switches=[])
        for k, v in sec['values']:
            key = canon.get(k.lower(), k)
            if key == 'CATEGORY': s['categories'].append(v)
            else: s[key] = v
        for k, v in MUSIC_DEFAULTS.items(): s.setdefault(k, v)
        s['categoryNames'] = [CATEGORY_NAMES.get(c, str(c)) for c in s['categories']]
        songs.append(s)
    return songs


# ------------------------------------------------------------------------------------------------
# Pathfinder .mpf v4 (loader EE 0x3D2350; node playback 0x3D41A8 / 0x3D3658; routers 0x3D3C90)

def al4(x): return (x + 3) & ~3


def parse_mpf(b):
    if b[:4] != b'xDFP' or b[4] != 4:
        raise ValueError('not a PFDx v4 file')
    nT, nS, nE, nR, nV = b[0xD], b[0xE], b[0xF], b[0x10], b[0x11]
    nN = struct.unpack_from('<H', b, 0x12)[0]
    node_off = [struct.unpack_from('<H', b, 0x20 + 2 * i)[0] * 4 for i in range(nN)]
    nodes = []
    for i, o in enumerate(node_off):
        sample, flags = struct.unpack_from('<hH', b, o)
        w4, w8, w12 = struct.unpack_from('<III', b, o + 4)
        nb = (w4 >> 12) & 0x1F
        branches = []
        for k in range(nb):
            lo, hi, target = struct.unpack_from('<BBH', b, o + 16 + 4 * k)
            branches.append(dict(lo=lo, hi=hi, node=target if target != 0xFFFF else -1))
        nodes.append(dict(index=i, offset=o, sample=sample, flags=flags, router=w4 & 0xFF,
                          w4=w4, measures=(w4 >> 20) & 0xF, beatsPerMeasure=(w4 >> 24) & 0xF,
                          w8=w8, syncMode=(w8 >> 8) & 3, sectionNode=w8 >> 20, w12=w12, branches=branches))
    last = node_off[-1]
    ev_table = al4(last + 0x10 + ((struct.unpack_from('<I', b, last + 4)[0] >> 10) & 0x7C))
    ev_off = [struct.unpack_from('<H', b, ev_table + 2 * i)[0] * 4 for i in range(nE)]
    events = []
    end_events = al4(ev_table + 2 * nE)
    for i, o in enumerate(ev_off):
        head = b[o:o + 16]
        eid, na = struct.unpack_from('<HH', head, 12)
        na &= 0x3F
        actions = []
        for k in range(na):
            a = b[o + 16 + 16 * k:o + 32 + 16 * k]
            actions.append(dict(raw=a.hex(), track=a[0], kind=a[3], op=a[9], value=struct.unpack_from('<h', a, 12)[0],
                                arg=a[14], arg2=a[15], u16_10=struct.unpack_from('<H', a, 10)[0]))
        events.append(dict(index=i, offset=o, id=eid, head=head[:12].hex(), actions=actions))
        end_events = al4(o + 16 + 16 * na)
    routers_at = end_events
    R = [struct.unpack_from('<I', b, routers_at + 4 * i)[0] for i in range(nR + 1)]
    routers = []
    for k in range(nR):
        pairs = [struct.unpack_from('<I', b, (R[k] + j) * 4)[0] for j in range(R[k + 1] - R[k])]
        routers.append([dict(fromNode=p >> 16, toNode=p & 0xFFFF) for p in pairs])
    tracks_at = R[-1] * 4
    T = [struct.unpack_from('<I', b, tracks_at + 4 * i)[0] for i in range(nT + 1)]
    samples_at = tracks_at + 4 * nT + 4
    samples, tracks = [], []
    for t in range(nT):
        first = (T[t] * 4 - samples_at) // 8; count = (T[t + 1] - T[t]) // 2
        tracks.append(dict(index=t, firstSample=first, sampleCount=count))
        for j in range(count):
            off, ms = struct.unpack_from('<II', b, T[t] * 4 + 8 * j)
            samples.append(dict(index=first + j, track=t, value=off, ms=ms))
    return dict(version=4, u6=struct.unpack_from('<H', b, 6)[0], counts=dict(tracks=nT, sections=nS, events=nE,
                routers=nR, vars=nV, nodes=nN), tables=dict(nodes=0x20, nodeData=0x20 + 2 * nN, events=ev_table,
                eventData=al4(ev_table + 2 * nE), routers=routers_at, tracks=tracks_at, samples=samples_at, end=T[-1] * 4),
                size=len(b), nodes=nodes, events=events, routers=routers, tracks=tracks, samples=samples)


def export_music(disc, out, only_songs=None):
    bigs = {1: BigOnDisc(disc, 'DATA/AUDIO/MUSIC.BIG'), 2: BigOnDisc(disc, 'DATA/AUDIO/MUSIC2.BIG')}
    songs = music_songs(disc.file('DATA/CONFIG/MUSIC.INF').decode('latin1'))
    folder = out / 'music'; folder.mkdir(parents=True, exist_ok=True)
    report, copied = [], {}

    def find(name):
        for k in (1, 2):
            if name.lower() in bigs[k].by_name: return k
        return None

    for s in songs:
        if only_songs and s['id'].lower() not in only_songs: continue
        mpf_name = s.get('PATHDATA'); mus_name = s.get('MUSDATA'); loop_name = s.get('LOOPDATA')
        if not mpf_name: continue
        k = find(mpf_name)
        mpf = parse_mpf(bigs[k].read(mpf_name))
        files = [mus_name] + ([loop_name] if loop_name else [])
        track_files = []
        for t, fname in enumerate(files[:mpf['counts']['tracks']]):
            kb = find(fname)
            data = bigs[kb].read(fname)
            kind = 'stream' if data[:4] == b'SCHl' else 'bank' if data[:4] == b'BNKl' else 'unknown'
            safe = re.sub(r'[^A-Za-z0-9_.-]', '_', fname)
            if safe not in copied:
                (folder / safe).write_bytes(data); copied[safe] = len(data)
            track_files.append(dict(file=safe, original=fname, big=('MUSIC.BIG' if kb == 1 else 'MUSIC2.BIG'),
                                    kind=kind, size=len(data), data=data))
        mismatch = 0
        for tr in mpf['tracks']:
            tf = track_files[tr['index']]
            tr.update(file=tf['file'], kind=tf['kind'], original=tf['original'], big=tf['big'])
            ts = [x for x in mpf['samples'] if x['track'] == tr['index']]
            if tf['kind'] == 'stream':
                for x in ts:
                    info = walk_stream(tf['data'], x['value'] * 0x80)
                    x.update(kind='stream', offset=x['value'] * 0x80, size=info['end'] - x['value'] * 0x80,
                             channels=info['channels'], sampleRate=info['sampleRate'], sampleCount=info['sampleCount'],
                             codec=info['codecName'], blocks=info['blocks'])
                    if 'tags' in info: x['tags'] = info['tags']
                    if x['ms'] != info['sampleCount'] * 1000 // info['sampleRate']: mismatch += 1
            elif tf['kind'] == 'bank':
                bank = parse_bank(tf['data'])
                tr['bankEntries'] = bank['count']
                starts = sorted(x['value'] for x in ts)
                for x in ts:
                    i0 = x['value']; nxt = [v for v in starts if v > i0]; stop = nxt[0] if nxt else bank['count']
                    total, cnt, rate, ch, n = 0, 0, None, None, 0
                    for i in range(i0, stop):
                        e = bank['entries'][i]
                        if e is None: continue
                        p = e['patches'][0]
                        if total >= x['ms'] - 2: break
                        total += p['sampleCount'] * 1000 / p['sampleRate']; cnt += 1; n += p['sampleCount']
                        rate, ch = p['sampleRate'], p['channels']
                    x.update(kind='bank', bankIndex=i0, count=cnt, sampleRate=rate, channels=ch, sampleCount=n,
                             codec=CODEC_NAMES[bank['entries'][i0]['patches'][0]['codec']])
                    if abs(total - x['ms']) > 2: mismatch += 1
        song = dict(id=s['id'], title=s.get('TITLE'), artist=s.get('ARTIST'), album=s.get('ALBUM'), inf=s,
                    mpf=dict(file=mpf_name, big='MUSIC.BIG' if k == 1 else 'MUSIC2.BIG'), **mpf)
        song['validation'] = dict(sampleDurationMismatches=mismatch)
        safe_id = re.sub(r'[^A-Za-z0-9_-]', '_', s['id'])
        (folder / f'{safe_id}.json').write_text(json.dumps(song, separators=(',', ':')))
        report.append(dict(id=s['id'], json=f'music/{safe_id}.json', title=s.get('TITLE'), artist=s.get('ARTIST'),
                           nodes=len(mpf['nodes']), samples=len(mpf['samples']), events=len(mpf['events']),
                           tracks=[dict(file=t['file'], kind=t['kind'], samples=t['sampleCount']) for t in mpf['tracks']],
                           mismatches=mismatch))
        print(f"music {s['id']:<12} nodes {len(mpf['nodes']):4d} samples {len(mpf['samples']):4d} "
              f"events {len(mpf['events']):3d} tracks {[t['kind'] for t in mpf['tracks']]} mismatches {mismatch}")
    return report, songs


# ------------------------------------------------------------------------------------------------
# speech (.hdr index tables + .dat stream concatenations)

SPEECH_CHARS = {'ari': 'Allegra', 'eli': 'Elise', 'grf': 'Griff', 'kao': 'Kaori', 'mac': 'Mac', 'mob': 'Moby',
                'nat': 'Nate', 'psy': 'Psymon', 'vig': 'Viggo', 'zoe': 'Zoe'}


def parse_hdr(h):
    """Speech line table (see docs/audio-formats.md#speech-hdr)."""
    nfields, count, count2, mask = h[4], h[5], h[6], h[7]
    unit = 256 << bin(mask).count('1')
    esz = 2 + nfields
    lines = []
    for i in range(count):
        p = 0x0C + esz * i
        lines.append(dict(offset=struct.unpack_from('>H', h, p)[0] * unit, fields=list(h[p + 2:p + esz])))
    tail = h[0x0C + esz * count:]
    return dict(id=struct.unpack_from('<H', h, 0)[0], fieldCount=nfields, count=count, count2=count2, alignMask=mask,
                unit=unit, datSize256=struct.unpack_from('<I', h, 8)[0], tail=tail.hex(), lines=lines)


def export_speech(disc, out, limit=None):
    folder = out / 'speech'; folder.mkdir(parents=True, exist_ok=True)
    report = []
    for big_path, head_name in (('DATA/AUDIO/ENGLISH.BIG', 'langhead.big'), ('DATA/AUDIO/SPEECH.BIG', 'headers.big')):
        big = BigOnDisc(disc, big_path)
        heads = big_entries(big.read(head_name))
        hb = big.read(head_name)
        hdr_by = {x['name'].split('\\')[-1].lower(): hb[x['offset']:x['offset'] + x['size']] for x in heads}
        for name in big.names('.dat'):
            if limit and len(report) >= limit: break
            data = big.read(name)
            stem = name[:-4]
            hdr = parse_hdr(hdr_by[stem.lower() + '.hdr'])
            streams = {x['offset']: x for x in all_streams(data)}
            lines = []
            for i, ln in enumerate(hdr['lines']):
                info = streams.get(ln['offset'])
                if info is None: raise ValueError(f'{name}: line {i} offset {ln["offset"]:#x} is not a stream')
                lines.append(dict(index=i, offset=ln['offset'], size=info['end'] - ln['offset'], fields=ln['fields'],
                                  channels=info['channels'], sampleRate=info['sampleRate'],
                                  sampleCount=info['sampleCount'], codec=info['codecName'],
                                  seconds=round(info['sampleCount'] / info['sampleRate'], 3)))
            m = re.match(r'^(.*?)_(eng|ari|eli|grf|kao|mac|mob|nat|psy|vig|zoe)$', stem, re.I)
            category, suffix = (m.group(1), m.group(2).lower()) if m else (stem, None)
            safe = re.sub(r'[^A-Za-z0-9_-]', '_', stem)
            (folder / f'{safe}.dat').write_bytes(data)
            doc = dict(file=f'{safe}.dat', original=name, big=big_path.split('/')[-1], category=category,
                       speaker=('DJ Atomika' if category.startswith('DJ_') else 'PA announcer' if category.startswith('PA_')
                                else SPEECH_CHARS.get(suffix, None)),
                       language='eng' if suffix == 'eng' else None, character=suffix if suffix != 'eng' else None,
                       size=len(data), header={k: v for k, v in hdr.items() if k != 'lines'}, lines=lines,
                       unindexedStreams=len(streams) - len(lines))
            (folder / f'{safe}.json').write_text(json.dumps(doc, separators=(',', ':')))
            report.append(dict(name=stem, json=f'speech/{safe}.json', lines=len(lines), category=category,
                               character=doc['character'], big=doc['big']))
        print(f'speech {big_path}: {sum(1 for r in report if r["big"] == big_path.split("/")[-1])} files')
    return report


# ------------------------------------------------------------------------------------------------
# sound banks

def export_banks(disc, out):
    folder = out / 'banks'; folder.mkdir(parents=True, exist_ok=True)
    report = []
    sources = [('AUDIO.BIG', n, None) for n in BigOnDisc(disc, 'DATA/AUDIO/AUDIO.BIG').names('')]
    sources += [('DISC', e['path'].split('/')[-1], e['path']) for e in disc.entries
                if re.match(r'DATA/AUDIO/GRNT_.*\.BNK$', e['path'], re.I)]
    audio_big = BigOnDisc(disc, 'DATA/AUDIO/AUDIO.BIG')
    for src, name, path in sources:
        data = disc.file(path) if path else audio_big.read(name)
        safe = re.sub(r'[^A-Za-z0-9_.-]', '_', name)
        (folder / safe).write_bytes(data)
        if data[:4] == b'BNKl':
            bank = parse_bank(data)
            codecs = sorted({p['codecName'] for e in bank['entries'] if e for p in e['patches']})
            doc = dict(file=safe, original=name, source=src, **bank)
            (folder / (Path(safe).stem + '.json')).write_text(json.dumps(doc, separators=(',', ':')))
            report.append(dict(name=name, file=f'banks/{safe}', json=f'banks/{Path(safe).stem}.json', kind='bank',
                               entries=sum(1 for e in bank['entries'] if e), codecs=codecs))
        else:
            report.append(dict(name=name, file=f'banks/{safe}', kind=data[:4].decode('latin1'), size=len(data)))
    print(f'banks: {len(report)} files')
    return report


# ------------------------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--iso', type=Path, default=ISO)
    ap.add_argument('--out', type=Path, default=OUT)
    ap.add_argument('--only', default='music,speech,banks', help='comma list of music,speech,banks')
    ap.add_argument('--songs', default='', help='comma list of MUSIC.INF section ids (default all)')
    ap.add_argument('--speech-limit', type=int, default=None, help='export only the first N speech files')
    args = ap.parse_args()
    only = set(args.only.split(','))
    args.out.mkdir(parents=True, exist_ok=True)
    disc = Disc(args.iso)
    try:
        cfg = {n: disc.file(f'DATA/CONFIG/{n}').decode('latin1') for n in
               ('MUSIC.INF', 'PLAYLIST.INF', 'MIX.INF', 'BANKS.INF', 'CROWD.INF', 'SPEECH.INF')}
        songs = music_songs(cfg['MUSIC.INF'])
        catalog_path = args.out / 'catalog.json'
        catalog = json.loads(catalog_path.read_text()) if catalog_path.exists() else {}
        catalog.update(
            provenance=dict(source='DATA/AUDIO/*.BIG, DATA/AUDIO/GRNT_*.BNK, DATA/CONFIG/*.INF',
                            decoder='web/audio-decode.js', formats='docs/audio-formats.md'),
            songs=songs,
            playlist=[dict(name=s['name'], songs=[v for k, v in s['values'] if k.upper() == 'SONG'])
                      for s in parse_inf(cfg['PLAYLIST.INF'])],
            mixes=[dict(name=s['name'], **{k: v for k, v in s['values']}) for s in parse_inf(cfg['MIX.INF'])],
            banksInf=[dict(name=s['name'], **{k: v for k, v in s['values']}) for s in parse_inf(cfg['BANKS.INF'])],
            crowdInf=[dict(name=s['name'], **{k: v for k, v in s['values']}) for s in parse_inf(cfg['CROWD.INF'])],
            speechInf=[dict(name=s['name'], **{k: v for k, v in s['values']}) for s in parse_inf(cfg['SPEECH.INF'])])
        songset = {x.strip().lower() for x in args.songs.split(',') if x.strip()} or None
        if 'music' in only:
            report, _ = export_music(disc, args.out, songset)
            if songset and 'music' in catalog:
                keep = [m for m in catalog['music'] if m['id'].lower() not in songset]
                report = keep + report
            catalog['music'] = report
        if 'speech' in only: catalog['speech'] = export_speech(disc, args.out, args.speech_limit)
        if 'banks' in only: catalog['banks'] = export_banks(disc, args.out)
        catalog_path.write_text(json.dumps(catalog, indent=1))
    finally:
        disc.close()
    print(f'wrote {args.out}')


if __name__ == '__main__':
    main()
