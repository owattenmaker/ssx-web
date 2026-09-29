#!/usr/bin/env python3
"""Convert the disc's reward videos (DATA/MOVIES/*.MPC) into browser-playable MP4 (read-only; docs/characters.md).

Source (user's own disc, never modified): DATA/MOVIES/INTRO.MPC ("SSX 3 Intro Video", reward video 0) and
DATA/MOVIES/MTNALIVE.MPC ("E3 Video", reward video 1) -- the paths career.json rewards.video items carry
(data/movies/intro.mpc, data/movies/mtnalive.mpc). The 130rewardposter/MoviePlayer state (0x1D23E0: creates
'MoviePlayer', starts the file, pauses the sound system 0x2B3A70 while it plays, pops back 0x1D2638 when the
movie ends or on a skip button 0x1D2518) plays them full screen.

MPC container: EA SCxl chunks, little-endian sizes.
  MPCh  one MPEG-2 video access unit per chunk (512x448, 29.97 fps, shown 4:3)
  SCHl  audio header 'GSTR' + 01000000 + EA PT elements: FD 85 n_samples, 82 channels, 84 rate (44000) FF
  SCCl  block count;  SCDl  one audio block per video frame: BE u32 samples, BE u32 offset per channel,
        then each channel's EA-XA R3 frames (0xEE = uncompressed frame);  SCEl end
INTRO.MPC carries 6 channels (L, C, R, Ls, Rs, LFE: INTRO_DJ.MPC differs from it only in channel 1, the DJ
voice, so channel 1 is the centre); MTNALIVE.MPC is stereo.

ffmpeg's 'ea' demuxer reads MPCh video but not this audio: the GSTR header has no revision element (so no
codec is chosen) and it refuses more than 2 channels. So each SCHl is rewritten as a standard 'PT' header with
revision 3 (EA-XA R3, big-endian block header), and a 6-channel stream is split into three stereo streams
(same SCDl samples, two channel offsets each). ffmpeg then decodes them; the six channels are folded to stereo
(L + 0.7071 C + 0.7071 Ls, R + 0.7071 C + 0.7071 Rs, LFE dropped; then scaled to peak at -1 dBFS if louder),
and the 512x448 picture is scaled to 640x480 (square pixels, 4:3 like the PS2 output).
The EA trailers of Main menu > Previews (146Bonusmat: NFSXSELL / NFLXSELL / ST3XSELL.MPC) carry `preview` =
their menu item (web/fe-previews.js).
Output (git-ignored): web/public/assets/MOVIES/<KEY>.mp4 (H.264 + AAC 48 kHz, faststart) and movies.json (merged:
exporting some keys keeps the other entries). Working files go to local/movies/ (git-ignored) and are removed afterwards.

ffmpeg: --ffmpeg PATH, else $SSX3_FFMPEG, else 'ffmpeg' on PATH, else the imageio_ffmpeg package's binary.
It needs the 'ea' demuxer, mpeg2video, adpcm_ea_r3, libx264 and aac.
"""
import argparse, json, os, re, shutil, struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_disc import Disc  # noqa: E402

from disc_paths import ps2_iso;ISO = ps2_iso()
OUT = ROOT / 'web/public/assets/MOVIES'
WORK = ROOT / 'local/movies'
MOVIES = {  # reward video index -> disc file (career.json rewards.video)
    'INTRO': dict(file='DATA/MOVIES/INTRO.MPC', reward='data/movies/intro.mpc', name='SSX 3 Intro Video'),
    'MTNALIVE': dict(file='DATA/MOVIES/MTNALIVE.MPC', reward='data/movies/mtnalive.mpc', name='E3 Video'),
    # Backcountry first-arrival movies (NIS list ids 29-31, 0x278E20; docs/cutscenes.md): silent (no SCHl), the
    # pktrans music stream plays over them. The WS files are the widescreen masters (0x4823DC table by display mode).
    'ABC1': dict(file='DATA/MOVIES/ABC1.MPC', reward=None, name='Peak 1 arrival (plane)'),
    'DBC2': dict(file='DATA/MOVIES/DBC2.MPC', reward=None, name='Peak 2 arrival'),
    'EBC3': dict(file='DATA/MOVIES/EBC3.MPC', reward=None, name='Peak 3 arrival'),
    'ABC1WS': dict(file='DATA/MOVIES/ABC1WS.MPC', reward=None, name='Peak 1 arrival (widescreen)'),
    'DBC2WS': dict(file='DATA/MOVIES/DBC2WS.MPC', reward=None, name='Peak 2 arrival (widescreen)'),
    'EBC3WS': dict(file='DATA/MOVIES/EBC3WS.MPC', reward=None, name='Peak 3 arrival (widescreen)'),
    # Main menu > Previews (146Bonusmat, cFEStateBonusMaterial): Cross on item i (0x195600, event 5) starts a
    # MoviePlayer (0x1D22F0) on the path table 0x441128[i] = data\movies\{nfs,nfl,st3}xsell.mpc (strings 0x460178..).
    'NFSXSELL': dict(file='DATA/MOVIES/NFSXSELL.MPC', reward=None, name='Need for Speed™ Underground', preview=0),
    'NFLXSELL': dict(file='DATA/MOVIES/NFLXSELL.MPC', reward=None, name='NFL STREET', preview=1),
    'ST3XSELL': dict(file='DATA/MOVIES/ST3XSELL.MPC', reward=None, name='NBA STREET Vol. 2', preview=2),
    # Boot and attract (the FE movie mask gp-0x1724, 0x1A27A0; path table 0x441248; web/fe-attract.js, docs/intro-movies.md):
    # the mask starts at 7 = EABIG (bit 0) -> THX (bit 1), neither skippable, -> the intro (bit 2, Start / Cross skip).
    # The intro is INTRO_DJ.MPC the first time (language 0 and the audio object's one-shot +0x6C88), INTRO.MPC after
    # (the title's 1801-frame idle attract, 0x1948A8, sets bit 2 again).
    'EABIG': dict(file='DATA/MOVIES/EABIG.MPC', reward=None, name='EA SPORTS BIG'),
    'THX': dict(file='DATA/MOVIES/THX.MPC', reward=None, name='THX'),
    'INTRO_DJ': dict(file='DATA/MOVIES/INTRO_DJ.MPC', reward=None, name='SSX 3 Intro Video (DJ)'),
}
DOWNMIX = 'pan=stereo|FL=c0+0.7071*c1+0.7071*c3|FR=c2+0.7071*c1+0.7071*c4'   # amerge keeps disc order: L C R Ls Rs LFE


def chunks(data):
    p = 0
    while p < len(data):
        tag = data[p:p + 4]; size = struct.unpack_from('<I', data, p + 4)[0]
        if size < 8 or p + size > len(data): raise ValueError(f'bad chunk {tag!r} at {p:#x}')
        yield tag, data[p:p + size]; p += size


def chunk(tag, body):
    while len(body) % 4: body += b'\0'
    return tag + struct.pack('<I', len(body) + 8) + body


def audio_header(data):
    """(channels, samples, rate) from the GSTR SCHl."""
    for tag, c in chunks(data):
        if tag != b'SCHl': continue
        if c[8:12] != b'GSTR': raise ValueError('expected a GSTR audio header')
        sub = c[c.index(b'\xfd', 12) + 1:]; out = {}; i = 0
        while i < len(sub) and sub[i] != 0xFF:
            k, n = sub[i], sub[i + 1]; out[k] = int.from_bytes(sub[i + 2:i + 2 + n], 'big'); i += 2 + n
        return out[0x82], out[0x85], out[0x84]
    return 0, 0, 0


def stereo_stream(data, pair):
    """The MPC with its audio reduced to channels 2*pair, 2*pair+1 and a PT/revision-3 header ffmpeg reads."""
    out = bytearray(); ch = None
    for tag, c in chunks(data):
        if tag == b'SCHl':
            sub = bytearray(c[c.index(b'\xfd', 12) + 1:c.rindex(b'\xff') + 1])
            j = sub.index(b'\x82\x01'); ch = sub[j + 2]; sub[j + 2] = 2
            c = chunk(b'SCHl', b'PT\0\0\xfd\x80\x01\x03' + bytes(sub))
        elif tag == b'SCDl':
            b = c[8:]; offs = list(struct.unpack_from(f'>{ch}I', b, 4)); base = 4 + 4 * ch
            ends = offs[1:] + [len(b) - base]
            a, r = (b[base + offs[k]:base + ends[k]] for k in (2 * pair, 2 * pair + 1))
            c = chunk(b'SCDl', b[:4] + struct.pack('>II', 0, len(a)) + a + r)
        out += c
    return bytes(out)


def find_ffmpeg(arg):
    for cand in (arg, os.environ.get('SSX3_FFMPEG'), shutil.which('ffmpeg')):
        if cand and Path(cand).exists(): return cand
    try:
        import imageio_ffmpeg; return imageio_ffmpeg.get_ffmpeg_exe()
    except Exception: pass
    sys.exit('export_movies: no ffmpeg found (pass --ffmpeg PATH or set SSX3_FFMPEG); nothing written')


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode: sys.exit(f'ffmpeg failed ({r.returncode}):\n{r.stderr[-2000:]}')
    return r.stderr


def export(disc, ffmpeg, key, spec, crf):
    data = disc.file(spec['file']); channels, samples, rate = audio_header(data)
    if channels == 0: return export_silent(disc, ffmpeg, key, spec, crf, data)
    if channels not in (2, 6): raise ValueError(f'{key}: {channels} audio channels')
    WORK.mkdir(parents=True, exist_ok=True)
    parts = []
    for pair in range(channels // 2):
        p = WORK / f'{key}_{pair}.mpc'; p.write_bytes(stereo_stream(data, pair)); parts.append(p)
    inputs = sum((['-i', str(p)] for p in parts), [])
    mix = f'[0:a][1:a][2:a]amerge=inputs=3,{DOWNMIX}' if channels == 6 else '[0:a]anull'
    # pass 1: the folded mix's peak
    err = run([ffmpeg, '-hide_banner', '-nostats', *inputs, '-filter_complex', f'{mix},volumedetect[a]', '-map', '[a]', '-f', 'null', '-'])
    peak = float(re.search(r'max_volume: (-?[\d.]+) dB', err).group(1))
    gain = min(0.0, -1.0 - peak)
    dst = OUT / f'{key}.mp4'; OUT.mkdir(parents=True, exist_ok=True)
    run([ffmpeg, '-hide_banner', '-nostats', '-y', *inputs, '-filter_complex',
         f'{mix},volume={gain:.2f}dB,aresample=48000[a];[0:v]scale=640:480:flags=lanczos,setsar=1[v]',
         '-map', '[v]', '-map', '[a]', '-c:v', 'libx264', '-preset', 'slow', '-crf', str(crf), '-pix_fmt', 'yuv420p',
         '-profile:v', 'high', '-c:a', 'aac', '-b:a', '192k', '-movflags', '+faststart', str(dst)])
    for p in parts: p.unlink()
    frames = sum(1 for t, _ in chunks(data) if t == b'MPCh')
    print(f'{key}: {frames} frames, {channels} ch {rate} Hz, {samples / rate:.2f} s, peak {peak} dB, gain {gain:.2f} dB -> {dst} ({dst.stat().st_size >> 20} MB)')
    return dict(key=key, name=spec['name'], reward=spec['reward'], src=f'MOVIES/{key}.mp4', frames=frames,
                fps=30000 / 1001, seconds=round(samples / rate, 3), channels=channels, rate=rate)


def export_silent(disc, ffmpeg, key, spec, crf, data):
    """A movie without an audio stream (the backcountry arrivals): video only."""
    WORK.mkdir(parents=True, exist_ok=True); src = WORK / f'{key}.mpc'; src.write_bytes(data)
    dst = OUT / f'{key}.mp4'; OUT.mkdir(parents=True, exist_ok=True)
    run([ffmpeg, '-hide_banner', '-nostats', '-y', '-f', 'ea', '-i', str(src), '-an', '-vf', 'scale=640:480:flags=lanczos,setsar=1',
         '-c:v', 'libx264', '-preset', 'slow', '-crf', str(crf), '-pix_fmt', 'yuv420p', '-profile:v', 'high', '-movflags', '+faststart', str(dst)])
    src.unlink()
    frames = sum(1 for t, _ in chunks(data) if t == b'MPCh')
    print(f'{key}: {frames} frames, silent -> {dst} ({dst.stat().st_size >> 20} MB)')
    return dict(key=key, name=spec['name'], reward=spec['reward'], src=f'MOVIES/{key}.mp4', frames=frames,
                fps=30000 / 1001, seconds=round(frames * 1001 / 30000, 3), channels=0, rate=0)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--iso', type=Path, default=ISO)
    ap.add_argument('--ffmpeg')
    ap.add_argument('--crf', type=int, default=18)
    ap.add_argument('--out', type=Path, default=None, help='output folder (default web/public/assets/MOVIES); movies.json is merged there')
    ap.add_argument('movies', nargs='*', default=list(MOVIES))
    a = ap.parse_args()
    global OUT
    if a.out: OUT = a.out
    ffmpeg = find_ffmpeg(a.ffmpeg); disc = Disc(a.iso)
    try:
        index = []
        manifest = OUT / 'movies.json'
        if manifest.exists(): index = [m for m in json.loads(manifest.read_text())['movies'] if m['key'] not in a.movies]
        for key in a.movies:
            m = export(disc, ffmpeg, key, MOVIES[key], a.crf)
            if MOVIES[key].get('preview') is not None: m['preview'] = MOVIES[key]['preview']   # Previews menu item
            index.append(m)
        manifest.write_text(json.dumps({'movies': sorted(index, key=lambda m: m['key'])}, indent=1))
    finally:
        disc.close()
        if WORK.exists() and not any(WORK.iterdir()): WORK.rmdir()


if __name__ == '__main__':
    main()
