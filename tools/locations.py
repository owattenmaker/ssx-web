#!/usr/bin/env python3
"""Per-location registry for the generalized course pipeline (docs/locations.md).

Every exporter that used to hard-code ARA1 takes a location code and resolves its
inputs/outputs here, so Snow Jam keeps its historical paths byte-for-byte while other
events get their own folders.

Identity comes from the executable, not from guesses:
* ELF location table 0x43E250 (24-byte {id, name[16], kind}; kind 0 event course,
  1 peak hub, 2 connector, 3 TRANSP, 4 sky): ARA1 id 0, BRA2 id 1, BHP1 id 11.
* ELF course table (file 0x33E950.., 0x64-byte rows {index, name[32], short[16],
  code[16], world[16], ...}): 0 "Snow Jam" ARA1, 1 "Metro-City" BRA2, 12 "The Junction"
  BHP1 (short name "Disfunk"). CMNAMER.LOC: "The Junction, a BEGINNER Super Pipe,
  happens in a fantastic city setting"; Metro-City is the Peak 1 BEGINNER street race.
* Peak 1 = areas A and B (ARA1, ABA1, ASS1, ABC1, BRA2, BHP1): same peak field 0.

SPDX-License-Identifier: GPL-3.0-only
"""
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / 'local/reference/pcsx2'
ELF = ROOT / 'local/disc/SLUS_207.72'

# states: derived ARMSX2 savestates of the EVENT (never free-ride):
#   countdown  grid countdown fixture (all riders control 6 / motion 3, race ticks 0)
#   anchor     countdown anchor used as the capture baseline for grid starts
#   glide      post-gate GROUNDED riding checkpoint (Cross on the race overlay, then ~360 frames; Metro-City is
#              airborne off the drop at that point, so its glide is the same run saved at tick 620, settled after the landing)
LOCATIONS = {
    'ARA1': dict(code='ARA1', name='Snow Jam', event='race', label='Snow Jam - Race', peak=1, location_id=0, course_index=0,
                 sky='ASKY', states=dict(countdown='countdown-1.p2m2_SaveState.p2s', anchor='snow-jam-countdown-anchor.p2s',
                                         glide='snow-jam-glide.p2s', ready='snow-jam-ready.p2s')),
    'BRA2': dict(code='BRA2', name='Metro-City', event='race', label='Metro-City - Race', peak=1, location_id=1, course_index=1,
                 sky='BSKY', states=dict(countdown='metro-city-countdown-anchor.p2s', anchor='metro-city-countdown-anchor.p2s',
                                         glide='metro-city-glide-620.p2s', ready='metro-city-ready.p2s')),
    'BHP1': dict(code='BHP1', name='The Junction', event='superpipe', label='The Junction - Super Pipe', peak=1, location_id=11,
                 course_index=11, sky='BSKY', states=dict(countdown='the-junction-countdown-anchor.p2s', anchor='the-junction-countdown-anchor.p2s',
                                                          glide='the-junction-glide.p2s', ready='the-junction-ready.p2s')),
    # Peak 1 backcountry (docs/backcountry.md): Single Event "Happiness" (mode 4 Rival Time) / "Happiness Jam"
    # (mode 5 Rival Points); the event savestates are the Rival Time ones (the rival is a computer rider). There is no
    # countdown (234AD0 -> 233AA0 for event kinds 4..6): the ready state (phase 3, rolling start) is the anchor
    'ABC1': dict(code='ABC1', name='Happiness', event='backcountry', label='Happiness', peak=1, location_id=14,
                 course_index=14, sky='ASKY', states=dict(countdown='happiness-ready.p2s', anchor='happiness-ready.p2s',
                                                          glide='happiness-glide.p2s', ready='happiness-ready.p2s')),
    # Peak 1 freestyle (docs/slopestyle-bigair.md): R&B slope style (mode 1, one computer opponent rides the course) and
    # Crow's Nest big air (mode 3, solo), both from peak-1-selection.p2s -> Freestyle -> Select Event (ps2_navigate)
    'ASS1': dict(code='ASS1', name='R&B', event='slopestyle', label='R&B - Slopestyle', peak=1, location_id=5,
                 course_index=5, sky='ASKY', states=dict(countdown='r-and-b-countdown-anchor.p2s', anchor='r-and-b-countdown-anchor.p2s',
                                                         glide='r-and-b-glide.p2s', ready='r-and-b-ready.p2s')),
    'ABA1': dict(code='ABA1', name="Crow's Nest", event='bigair', label="Crow's Nest - Big Air", peak=1, location_id=8,
                 course_index=8, sky='ASKY', states=dict(countdown='crows-nest-countdown-anchor.p2s', anchor='crows-nest-countdown-anchor.p2s',
                                                         glide='crows-nest-glide.p2s', ready='crows-nest-ready.p2s')),
    # ---- Peak 2 (docs/peak2.md): areas C and D, course table +0x54 = 1. Savestates are derived from
    # peak-1-selection.p2s with the Peak 2 pass bit (+0x278 bit 12) cleared, then Select Peak -> Peak 2 (ps2_navigate).
    'CRA3': dict(code='CRA3', name='Ruthless Ridge', event='race', label='Ruthless Ridge - Race', peak=2, location_id=2, course_index=2,
                 sky='CSKY', states=dict(countdown='ruthless-ridge-countdown-anchor.p2s', anchor='ruthless-ridge-countdown-anchor.p2s',
                                         glide='ruthless-ridge-glide.p2s', ready='ruthless-ridge-ready.p2s')),
    'DRA4': dict(code='DRA4', name='Intimidator', event='race', label='Intimidator - Race', peak=2, location_id=3, course_index=3,
                 sky='DSKY', states=dict(countdown='intimidator-countdown-anchor.p2s', anchor='intimidator-countdown-anchor.p2s',
                                         glide='intimidator-glide.p2s', ready='intimidator-ready.p2s')),
    'DSS2': dict(code='DSS2', name='Style Mile', event='slopestyle', label='Style Mile - Slopestyle', peak=2, location_id=6, course_index=6,
                 sky='DSKY', states=dict(countdown='style-mile-countdown-anchor.p2s', anchor='style-mile-countdown-anchor.p2s',
                                         glide='style-mile-glide.p2s', ready='style-mile-ready.p2s')),
    'CBA2': dict(code='CBA2', name='Launch Time', event='bigair', label='Launch Time - Big Air', peak=2, location_id=9, course_index=9,
                 sky='CSKY', states=dict(countdown='launch-time-countdown-anchor.p2s', anchor='launch-time-countdown-anchor.p2s',
                                         glide='launch-time-glide.p2s', ready='launch-time-ready.p2s')),
    'CHP2': dict(code='CHP2', name='Schizophrenia', event='superpipe', label='Schizophrenia - Super Pipe', peak=2, location_id=12,
                 course_index=12, sky='CSKY', states=dict(countdown='schizophrenia-countdown-anchor.p2s', anchor='schizophrenia-countdown-anchor.p2s',
                                                          glide='schizophrenia-glide.p2s', ready='schizophrenia-ready.p2s')),
    # Peak 2 backcountry: Single Event "Ruthless" (mode 4 Rival Time) / "Ruthless Jam" (mode 5); rival Nate (Zoe for Nate).
    # Rolling start like Happiness: the ready state is the anchor.
    'DBC2': dict(code='DBC2', name='Ruthless', event='backcountry', label='Ruthless', peak=2, location_id=15,
                 course_index=15, sky='DSKY', states=dict(countdown='ruthless-ready.p2s', anchor='ruthless-ready.p2s',
                                                          glide='ruthless-glide.p2s', ready='ruthless-ready.p2s')),
    # ---- Peak 3 (docs/peak3.md): area E, course table +0x54 = 2, sky ESKY. Savestates are derived from
    # peak-1-selection.p2s with the lock words +0x278 bits 6..19 cleared (every character block), then Triangle / Cross
    # (the Select Peak list reads the locks on entry), Select Peak -> Peak 3 (local/ps2-capture/peak3/nav).
    'ERA5': dict(code='ERA5', name='Gravitude', event='race', label='Gravitude - Race', peak=3, location_id=4, course_index=4,
                 sky='ESKY', states=dict(countdown='gravitude-countdown-anchor.p2s', anchor='gravitude-countdown-anchor.p2s',
                                         glide='gravitude-glide.p2s', ready='gravitude-ready.p2s')),
    'ESS3': dict(code='ESS3', name='Kick Doubt', event='slopestyle', label='Kick Doubt - Slopestyle', peak=3, location_id=7, course_index=7,
                 sky='ESKY', states=dict(countdown='kick-doubt-countdown-anchor.p2s', anchor='kick-doubt-countdown-anchor.p2s',
                                         glide='kick-doubt-glide.p2s', ready='kick-doubt-ready.p2s')),
    'EBA3': dict(code='EBA3', name='Much-2-Much', event='bigair', label='Much-2-Much - Big Air', peak=3, location_id=10, course_index=10,
                 sky='ESKY', states=dict(countdown='much-2-much-countdown-anchor.p2s', anchor='much-2-much-countdown-anchor.p2s',
                                         glide='much-2-much-glide.p2s', ready='much-2-much-ready.p2s')),
    'EHP3': dict(code='EHP3', name='Perpendiculous', event='superpipe', label='Perpendiculous - Super Pipe', peak=3, location_id=13,
                 course_index=13, sky='ESKY', states=dict(countdown='perpendiculous-countdown-anchor.p2s', anchor='perpendiculous-countdown-anchor.p2s',
                                                          glide='perpendiculous-glide.p2s', ready='perpendiculous-ready.p2s')),
    # Peak 3 backcountry: Single Event "The Throne" (mode 4 Rival Time) / "Throne Jam" (mode 5); rival Psymon (Elise for
    # Psymon). Rolling start like Happiness: the ready state is the anchor.
    'EBC3': dict(code='EBC3', name='The Throne', event='backcountry', label='The Throne', peak=3, location_id=16,
                 course_index=16, sky='ESKY', states=dict(countdown='the-throne-ready.p2s', anchor='the-throne-ready.p2s',
                                                          glide='the-throne-glide.p2s', ready='the-throne-ready.p2s')),
}


def location(code):
    if code not in LOCATIONS:
        raise ValueError(f'Unsupported location {code}; known: {", ".join(LOCATIONS)}')
    return LOCATIONS[code]


def state(code, kind):
    """Path of a location's reference savestate (may not exist yet)."""
    return REFERENCE / location(code)['states'][kind]


def native_dir(code):
    return ROOT / 'local/assets/native' / code


def web_dir(code):
    return ROOT / 'web/public/assets' / code


def activation_dir(code):
    """Event-activation evidence (countdown instance audit, scripted instances, fog tree).

    Snow Jam keeps the historical flat folder; other locations get a subfolder."""
    base = ROOT / 'local/event-activation'
    return base if code == 'ARA1' else base / code


def pickups_dir(code):
    base = ROOT / 'local/browser-pickups'
    return base if code == 'ARA1' else base / code


def pickup_file(code, stem):
    """ARA1 keeps local/browser-pickups/ara1-<stem>.json; others <code>/<stem>.json."""
    return pickups_dir(code) / (f'ara1-{stem}.json' if code == 'ARA1' else f'{stem}.json')


def web_manifest_entry(code):
    """Browser asset roots for a course (web/public/assets/courses.json)."""
    info = location(code)
    if code == 'ARA1':
        roots = dict(root='/assets/ARA1/', sky='/assets/SKY/', sunFlare='/assets/SUN_FLARE/', lightGlow='/assets/LIGHT_GLOW/',
                     # the start-gate spark fountains are drawn by the stage-world particle effects (program 118;
                     # web/stage_world.inc, web/set-piece-particles.js), not by the older startfire approximation
                     startfire=None, initial='/assets/ANIMATIONS/initial.json')
    else:
        root = f'/assets/{code}/'
        roots = dict(root=root, sky=root + 'sky/', sunFlare=root + 'sun-flare/', lightGlow=root + 'light-glow/',
                     startfire=None, initial=root + 'initial.json')
    return dict(code=code, name=info['name'], event=info['event'], label=info['label'], peak=info['peak'], **roots)


def elf_read(address, size, elf=None):
    elf = elf if elf is not None else ELF.read_bytes()
    ph = struct.unpack_from('<I', elf, 28)[0]; stride, count = struct.unpack_from('<HH', elf, 42)
    for i in range(count):
        typ, offset, base, _, length, _, _, _ = struct.unpack_from('<8I', elf, ph + i * stride)
        if typ == 1 and base <= address and address + size <= base + length:
            return elf[offset + address - base:offset + address - base + size]
    raise ValueError('Unmapped ELF address')


def verify_identity(code):
    """Check the registry against the executable's location table (id/kind)."""
    elf = ELF.read_bytes(); info = location(code)
    entry = elf_read(0x43E250 + 24 * info['location_id'], 24, elf)
    name = entry[4:20].split(b'\0')[0].decode(); kind = struct.unpack_from('<I', entry, 20)[0]
    if name != code or kind != 0:
        raise ValueError(f'ELF location table entry {info["location_id"]} is {name}/{kind}, expected {code}/0')
    return True


GP = 0x4A30F0


def human_rider(memory):
    """Address of the human rider actor in an event savestate's EE memory.

    Same path as tools/reference_race_event.py: world = *(*(gp-0x848)+0x84), game = world+12,
    human owners at game+0x40.. (rider = owner+0x18); checked against the human vtables
    (rider+0x6C0/+0x6E8 = 0x4583A8/0x458360). Snow Jam: 0x14701A0."""
    u = lambda a: struct.unpack_from('<I', memory, a)[0]
    world = u(u(GP - 0x848) + 0x84); game = u(world + 12)
    if u(game + 0xCC) != 0x458488:
        raise ValueError('Unexpected original game-info interface')
    if not 1 <= u(game + 0x7C) <= 4:
        raise ValueError('No human rider in this savestate')
    rider = u(u(game + 0x40) + 0x18)
    if (u(rider + 0x6C0), u(rider + 0x6E8)) != (0x4583A8, 0x458360):
        raise ValueError('Human rider vtables differ')
    return rider


if __name__ == '__main__':
    import json
    for code in LOCATIONS:
        verify_identity(code)
        print(json.dumps(web_manifest_entry(code)))
