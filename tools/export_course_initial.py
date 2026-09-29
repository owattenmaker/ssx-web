#!/usr/bin/env python3
"""Write web/public/assets/<LOC>/initial.json for a non-Snow-Jam course (docs/locations.md).

The browser's initial.json mixes rider state (Zoe's recovered profiles, captured in the Snow Jam
glide savestate) with course state. A course file is the Snow Jam file with the course keys
replaced:

* original_race_event paths  <- the course's authored AIP track paths (SSB kind 14, rid 0),
* original_reset paths       <- the course's AIP AI paths,
* original_pickups           <- the course's own pickups (empty until its glide savestate exists),
* position/heading           <- web/public/assets/<LOC>/start.json.

For Snow Jam the runtime race/reset path banks equal the AIP records exactly (origin, bounds,
segments, event start/end/value; race remaining_at_origin = track field2, reset +0x38 = ai
field3, +0x3C = ai field6) except for the event type, which the loader renumbers. Only the
renumbering observed in Snow Jam is known (race 0->1 finish, 18->11 checkpoint; reset -1->0, 100->12,
102->14, 103->15, 110->16, 111->17, 300->20; this reproduces both Snow Jam banks exactly);
unknown types are kept raw and listed. Everything not replaced still comes from Snow Jam and is
listed under 'provisional' until the course's own event savestates replace this file.
"""
import argparse, hashlib, json, math, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import world_chunks, records, locations  # noqa: E402
from race_event_assets import decode_aip  # noqa: E402
from locations import location as location_info, web_dir  # noqa: E402

RACE_EVENT_TYPES = {0: 1, 18: 11}      # AIP track-path event -> runtime course event (Snow Jam evidence)
RESET_EVENT_TYPES = {0xFFFFFFFF: 0, 100: 12, 102: 14, 103: 15, 110: 16, 111: 17, 300: 20}  # AIP AI-path -> runtime reset-path event (Snow Jam evidence)


def course_aip(code):
    track = next(i for i, l in enumerate(locations(ROOT / 'local/assets/source/ps2/bam.sdb')) if l['name'] == code)
    for chunk in world_chunks(ROOT / 'local/assets/source/ps2/bam.ssb'):
        for kind, t, rid, data in records(chunk):
            if kind == 14 and t == track and rid == 0:
                return decode_aip(data), hashlib.sha256(data).hexdigest()
    raise ValueError(f'{code}: no authored AIP record')


def remap(events, table, unknown):
    out = []
    for e in events:
        if e['type'] not in table:
            unknown.add(e['type'])
        out.append(dict(e, type=table.get(e['type'], e['type'])))
    return out


def nearest_path(paths, point):
    return min(range(len(paths)), key=lambda i: sum((a - b) ** 2 for a, b in zip(paths[i]['origin'], point)))


def export(code):
    if code == 'ARA1':
        raise ValueError('Snow Jam uses web/public/assets/ANIMATIONS/initial.json (web/prepare-ui.py)')
    location_info(code)
    base = json.loads((ROOT / 'web/public/assets/ANIMATIONS/initial.json').read_text())
    start = json.loads((web_dir(code) / 'start.json').read_text())
    aip, aip_sha = course_aip(code)
    unknown_race, unknown_reset = set(), set()
    race_paths = [dict(index=p['index'], origin=p['position'], low=p['low'], high=p['high'], remaining_at_origin=p['field2'],
                       segments=p['segments'], events=remap(p['events'], RACE_EVENT_TYPES, unknown_race)) for p in aip['track_paths']]
    reset_paths = [dict(origin=p['position'], low=p['low'], high=p['high'], segments=p['segments'],
                        events=remap(p['events'], RESET_EVENT_TYPES, unknown_reset), flags38=p['fields'][3], field3c=p['fields'][6])
                   for p in aip['ai_paths']]
    # Source (Z-up cm) spawn point from the native start (x, y_up, z) = (x, z, -y) / 100.
    x, y, z = start['position']; point = [x * 100, -z * 100, y * 100]
    initial = dict(base)
    initial['position'] = start['position']; initial['heading'] = start['heading']; initial['velocity'] = [0.0, 0.0, 0.0]
    race = json.loads(json.dumps(base['original_race_event']))
    event = race['original_race_event']; event['paths'] = race_paths
    human = next(p for p in event['participants'] if p['human'])
    k = nearest_path(race_paths, point); human.update(path_index=k, remaining=race_paths[k]['remaining_at_origin'],
                                                     best_remaining=race_paths[k]['remaining_at_origin'],
                                                     path_cache=dict(origin=race_paths[k]['origin'], distance=0.0, segment=0))
    event['participants'] = [human]
    event['provenance'] = dict(event['provenance'], course_sha256=hashlib.sha256(json.dumps(race_paths, separators=(',', ':')).encode()).hexdigest(),
                               aip_sha256=aip_sha, source=f'{code} authored AIP track paths (provisional; see tools/export_course_initial.py)')
    initial['original_race_event'] = race
    reset = json.loads(json.dumps(base['original_reset']))
    reset['paths'] = reset_paths
    r = nearest_path(reset_paths, point); o = reset_paths[r]['origin']
    route = dict(path_index=r, cache=dict(origin=o, distance=0.0, segment=0), closest_point=o, lookahead_point=o,
                 previous_distance=0.0, current_distance=0.0, lateral_distance=0.0, heading=start['heading'])
    reset['route'] = route; reset['event_route'] = dict(route)
    initial['original_reset'] = reset
    initial['original_pickups'] = dict(base['original_pickups'], items=[])
    initial['provisional'] = dict(
        location=code, reason=f'no {code} event savestate yet (tools/locations.py states)',
        course_from_disc=['original_race_event.paths', 'original_reset.paths', 'position', 'heading'],
        unknown_event_types=dict(race=sorted(unknown_race), reset=sorted(unknown_reset)),
        snow_jam_values=['rider profiles/state (Zoe, Snow Jam glide)', 'race clock/checkpoints', 'reset route cache (nearest path origin)',
                         'original_breath regions', 'original_environment ambient', 'original_snow/board-trail environment'],
        empty=['original_pickups.items'])
    out = web_dir(code) / 'initial.json'; out.write_text(json.dumps(initial))
    print(json.dumps(dict(output=str(out), race_paths=len(race_paths), reset_paths=len(reset_paths), human_path=k, reset_path=r,
                          unknown_event_types=initial['provisional']['unknown_event_types'])))


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('location'); export(p.parse_args().location)
