#!/usr/bin/env python3
"""Original EA MicroTalk decoder (0x3CDDB8/0x3CDDE8/0x3CDE68 over 0x3CD6F0/0x3CD878, development-only instruction
oracle, tests/microtalk_reference.cpp) on every MicroTalk bank patch of web/public/assets/AUDIO/banks.

Jobs follow the SND main-RAM voice (0x3C9420 open, 0x3C9520 loop restart): a one-shot patch is one stream of
sampleCount samples; a looped patch is the intro stream (loopStart samples at dataOffsets[c]) and the loop body, a
fresh stream (header parsed again) of loopEnd - loopStart + 1 samples at dataOffsets[c] + tag 0x1A (channel 0) /
0x26 (channel 1), both relative to dataOffsets[0]. Writes local/reference/microtalk/<bank>.f32 (raw decoder floats, jobs back to back) and
local/reference/microtalk/jobs.json; web/test-microtalk.mjs compares web/audio-decode.js against them.

Music streams (SCxl segments with codec2 4, i.e. "Screw Up"): every segment and channel runs through the original stream
voice read 0x3C96F0 (sub_003C92F8 entered at an added label; mode = tag 0x80 < 3 -> plain MicroTalk, 0x3C9844 / 0x3C97A8),
with the EE ring buffer fetch 0x3C6DE0 stubbed to hand out the SCDl blocks as 0x3B6EA8 describes them (count, channel data
pointer, bit 31 = "not the first block of this SCHl"). Reads are 1000 samples at a time (not frame aligned).
Writes local/reference/microtalk/music-<song>.f32 and the "music" section of jobs.json.
"""
from pathlib import Path
import hashlib, json, subprocess
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
from inspect_disc import EXPECTED_SHA1
root = Path(__file__).resolve().parents[1]; vendor = root / 'local/vendor/PS2Recomp'; build = root / 'build/ps2recomp'
elf = root / 'local/disc/SLUS_207.72'
if hashlib.sha1(elf.read_bytes()).hexdigest() != EXPECTED_SHA1: raise ValueError('Unexpected original SSX3 executable')
includes = [vendor / 'ps2xRuntime/include', vendor / 'ps2xRuntime/src/lib/Kernel', vendor / 'ps2xIOP/include', build / '_deps/sse2neon-src', root / 'local/output']
command = ['xcrun', 'clang++', '-std=c++20', '-O2', '-arch', 'arm64', '-DUSE_SSE2NEON', '-frounding-math', '-ffp-contract=off'] + ['-I' + str(p) for p in includes]
command += [str(root / 'tests/microtalk_reference.cpp')]
work = root / 'local/reference/microtalk'
for prefix in ('003CD1F8', '003CD260', '003CD2B0', '003CD518', '003CD590', '003CD690', '003CD6F0', '003CD878', '003CDDB8', '003CDDE8', '003CDE68',
               '003CE078', '003CE0B8', '003CE0D8', '003CE410', '0041605C'):
    path = next((root / 'local/output').glob('sub_' + prefix + '*'))
    command.append(str(write_scalar_fp_oracle(path, work / 'src' / ('microtalk-' + path.name), path.read_text())))
# The stream voice functions (0x3C9420 / 0x3C9520 / 0x3C9618 / 0x3C96F0 / 0x3C9960 ...) are one generated function
# 0x3C92F8..0x3C9DA0 without an entry label for the read 0x3C96F0: the oracle copy gets one (the code is unchanged).
voice = next((root / 'local/output').glob('sub_003C92F8*')); text = voice.read_text()
anchor = '    // 0x3c96f0: 0x27bdff80'
if text.count(anchor) != 1 or text.count('    switch (ctx->pc) {\n') != 1: raise ValueError('0x3C96F0 entry not found')
text = text.replace(anchor, 'label_3c96f0:\n' + anchor).replace('    switch (ctx->pc) {\n', '    switch (ctx->pc) {\n        case 0x3c96f0u: goto label_3c96f0;\n')
command.append(str(write_scalar_fp_oracle(voice, work / 'src' / ('microtalk-' + voice.name), text)))
path = next((root / 'local/output').glob('sub_00416210*'))
command.append(str(write_scalar_fp_oracle(path, work / 'src' / ('microtalk-' + path.name), path.read_text())))
command += [str(build / 'ps2xRuntime/libps2_runtime.a'), str(build / '_deps/raylib-build/raylib/libraylib.a'), str(build / 'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL', 'Cocoa', 'IOKit', 'CoreFoundation']: command += ['-framework', framework]
binary = root / 'build/ssx3_microtalk_reference'; command += ['-o', str(binary)]
cached_oracle_build(command, root / 'build/original-microtalk-objects')
import sys
if '--build-only' in sys.argv: sys.exit(0)

banks = root / 'web/public/assets/AUDIO/banks'; manifest = {}; total = 0
def param(p, tag): return p['params'].get('0x%02x' % tag)
for js in sorted(banks.glob('*.json')):
    doc = json.loads(js.read_text())
    if 'entries' not in doc: continue
    jobs = []
    for e in doc['entries']:
        if not e: continue
        for k, p in enumerate(e['patches']):
            if p['codec'] != 4: continue
            if p['tags'].get('0x8c', 4) < 3: raise ValueError(f'{js.name} {e["index"]}/{k}: MicroTalk without the EA patch mode')
            for c in range(p['channels']):
                tag = param(p, 0x1a if c == 0 else 0x26)
                base = p['dataOffsets'][c]
                if p['loopStart'] >= 0 and tag is not None:
                    intro = min(p['loopStart'], p['sampleCount'])
                    last = min(p['sampleCount'], p['loopEnd'] + 1) if p['loopEnd'] >= intro else p['sampleCount']
                    if intro > 0: jobs.append(dict(entry=e['index'], patch=k, channel=c, part='intro', offset=base, start=0, count=intro))
                    jobs.append(dict(entry=e['index'], patch=k, channel=c, part='body', offset=p['dataOffsets'][0] + tag, start=intro, count=last - intro))
                else:
                    jobs.append(dict(entry=e['index'], patch=k, channel=c, part='whole', offset=base, start=0, count=p['sampleCount']))
    if not jobs: continue
    out = work / (js.stem + '.f32')
    run = subprocess.run([str(binary), str(elf), str(banks / doc['file']), str(out)], input=''.join(f"{j['offset']} {j['count']}\n" for j in jobs),
                         capture_output=True, text=True, timeout=600)
    if run.returncode: raise RuntimeError(f'{js.name}: {run.stderr or run.stdout}')
    if 'missing-target' in run.stdout + run.stderr: raise RuntimeError('Incomplete original MicroTalk call graph')
    manifest[js.stem] = dict(file=doc['file'], reference=out.name, jobs=jobs); total += len(jobs)
print(f'{total} MicroTalk streams in {len(manifest)} banks decoded by the original -> {work}')

import struct
def stream_layout(u8, at):
    """SCHl tag 0x80 (0 when absent: the voice's header struct is cleared, 0x3BB820) and the SCDl blocks of the stream at `at`."""
    if u8[at:at+4] != b'SCHl' or u8[at+8:at+10] != b'PT': raise ValueError('SCHl expected')
    p, end, version = at + 12, at + struct.unpack_from('<I', u8, at + 4)[0], 0
    while p < end:
        tag = u8[p]; p += 1
        if tag in (0xFC, 0xFD, 0xFE): continue
        if tag == 0xFF: break
        n = u8[p]; p += 1
        if n == 0xFF: n = struct.unpack_from('>I', u8, p)[0]; p += 4
        if tag == 0x80: version = int.from_bytes(u8[p:p+n], 'big')
        p += n
    p, blocks = end, []
    while True:
        cc, size = u8[p:p+4], struct.unpack_from('<I', u8, p + 4)[0]
        if cc == b'SCDl': blocks.append(p)
        if cc == b'SCEl' or size < 8: break
        p += size
    return version, blocks
music = root / 'web/public/assets/AUDIO/music'; songs = {}; streams = 0
for js in sorted(music.glob('*.json')):
    doc = json.loads(js.read_text())
    if not isinstance(doc, dict) or 'samples' not in doc: continue
    segs = [x for x in doc['samples'] if x.get('kind') == 'stream' and x.get('codec') == 'microtalk']
    if not segs: continue
    jobs, lines = [], []
    for seg in segs:
        track = next(t for t in doc['tracks'] if t['index'] == seg['track'])
        u8 = (music / track['file']).read_bytes()[seg['offset']:seg['offset'] + seg['size']]
        version, blocks = stream_layout(u8, 0)
        ch = seg['channels']
        for c in range(ch):
            parts = []
            for b in blocks:
                n = struct.unpack_from('<I', u8, b + 8)[0]
                parts.append((n, b + 12 + 4 * ch + struct.unpack_from('<I', u8, b + 12 + 4 * c)[0]))
            count = sum(n for n, _ in parts)
            jobs.append(dict(sample=seg['index'], file=track['file'], channel=c, count=count, version=version))
            lines.append(f"{seg['offset']} {seg['size']} {version} {c} 1000 {len(parts)} " + ' '.join(f'{n} {o}' for n, o in parts))
    files = {j['file'] for j in jobs}
    if len(files) != 1: raise ValueError(f'{js.name}: MicroTalk segments in several files')
    out = work / ('music-' + js.stem + '.f32')
    run = subprocess.run([str(binary), str(elf), str(music / files.pop()), str(out), '--stream'], input='\n'.join(lines) + '\n',
                         capture_output=True, text=True, timeout=1800)
    if run.returncode: raise RuntimeError(f'{js.name}: {run.stderr or run.stdout}')
    songs[js.stem] = dict(reference=out.name, jobs=jobs); streams += len(jobs)
(work / 'jobs.json').write_text(json.dumps(dict(elf_sha1=EXPECTED_SHA1, banks=manifest, music=songs), separators=(',', ':')))
print(f'{streams} MicroTalk music segment channels in {len(songs)} songs decoded by the original stream voice -> {work}')
