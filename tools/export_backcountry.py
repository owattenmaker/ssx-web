#!/usr/bin/env python3
"""Backcountry (Happiness ABC1) rival-event evidence from the rolling-start savestate (docs/backcountry.md).

The rival challenges have no countdown: 234AD0 sends event kinds 4..6 (Rival Time/Points, free ride, peak
challenges) to 233AA0 instead of 113B10(Countdown), so the ready state (race clock phase 3, PreRace) already holds
both riders in their start state: control 0, motion 0, at the authored start points with a start velocity
(rolling start). On the Cross that closes the objectives overlay the clock goes straight to Race and both riders
fall onto the slope on tick 0.

    .venv/bin/python tools/export_backcountry.py event-start [--location ABC1]
        local/assets/native/ABC1/event-start.json (tools/export_event_start.py layout, `rolling_start: true`)
    .venv/bin/python tools/export_backcountry.py npc [--location ABC1]
        local/assets/native/ABC1/npc-riders.json and web/public/assets/ABC1/npc-riders.json
        (tools/export_npc_riders.py extract_document: the rival's settings, ground and NPC provider state)

Identity comes from the race copy 0x535B20 (+0x11 gameplay character, +0x12 cheat id) as in
tools/export_lineups.py; the rival is the peak rival 145750 (slot 1, GameModeMan +0x18[1]).
"""
import argparse, hashlib, json, struct, sys, zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from reference_race_event import extract_race_event  # noqa: E402
from reference_ground_profile import extract_ground  # noqa: E402
from locations import state as location_state  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
ROSTER = json.loads((ROOT / 'web/public/assets/riders.json').read_text())
NAME_OF = {(e['kind'] == 'cheat', e['character']): e['id'] for e in ROSTER if e['kind'] in ('rider', 'cheat')}
GP = 0x4A30F0


def rolling_participants(memory):
    race = extract_race_event(memory)
    clock = race['clock']
    if clock['phase'] != 3 or clock['race_ticks'] != 0 or clock['total_ticks'] != 0 or clock['countdown_ticks'] != 0:
        raise ValueError('not a rolling-start ready state (phase 3, tick 0)')
    u = lambda at: struct.unpack_from('<I', memory, at)[0]
    game = int(race['provenance']['game_address'], 16)
    out = []
    for actor in race['participants']:
        slot = actor['index']; address = u(game + 0x28 + slot * 4)
        if actor['control_state'] != 0 or actor['motion_mode'] != 0:
            raise ValueError('rider is not in the rolling start state (control 0, motion 0)')
        owner = u(address + 0x77C); phase, steady, low, high, pose, rider = struct.unpack_from('<I4fI', memory, owner + 0x290)
        entry = u(0x5305B0 + slot * 4)
        base, cheat = memory[0x535B20 + entry * 28 + 17], memory[0x535B20 + entry * 28 + 18]
        ground = extract_ground(memory, address)
        if ground['provenance']['character_id'] != base: raise ValueError('gameplay identity differs from the race copy')
        name = NAME_OF[(True, cheat)] if cheat else NAME_OF[(False, base)]
        out.append(dict(slot=slot, character=name, gameplay_character_id=base, render_package='RIDER_' + name.upper(), race=actor,
                        original_ground=ground, start_control=dict(phase=phase, steady_time=steady, low=low, high=high, pose=pose),
                        progress_origin=struct.unpack_from('<f', memory, address + 0x4D8)[0],
                        start_delay_seconds=struct.unpack_from('<i', memory, address + 0xB30)[0], reference_stance=u(address + 0x324)))
    return race, out


def game_mode(memory):
    return dict(course=struct.unpack_from('<i', memory, 0x535C08)[0], kind=memory[0x535C10], path=memory[0x535C11], mode=memory[0x535C12])


def event_start(code, snapshot=None, output=None):
    # snapshot / output: another ready state of the same event and where its export goes (tools/export_exact_event_starts.py:
    # the exact-derived ready state, local/assets/native-exact/<code>/event-start.json); by default the location's own.
    snapshot = Path(snapshot) if snapshot else location_state(code, 'countdown')
    memory = zipfile.ZipFile(snapshot).read('eeMemory.bin'); digest = hashlib.sha256(memory).hexdigest()
    race, participants = rolling_participants(memory)
    mode = game_mode(memory)
    if mode['mode'] not in (4, 5): raise ValueError(f'not a rival challenge state: {mode}')
    course = json.loads((ROOT / f'web/public/assets/{code}/initial.json').read_text())['original_race_event']['original_race_event']
    if course['provenance']['course_sha256'] != race['provenance']['course_sha256']: raise ValueError('ready-state course differs from current event paths')
    result = dict(version=1, location=code, rolling_start=True, game_mode=mode, clock=race['clock'], participants=participants,
                  configuration=race['provenance']['configuration_bytes'], course_sha256=race['provenance']['course_sha256'],
                  provenance=dict(snapshot=str(snapshot), ee_sha256=digest,
                                  purpose='Rolling-start ready state (no countdown) and source profiles; every later tick runs natively'))
    out = Path(output) if output else ROOT / f'local/assets/native/{code}/event-start.json'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(dict(output=str(out), mode=mode, clock=race['clock'], riders=[p['character'] for p in participants]), indent=1))


def npc(code):
    from export_npc_riders import extract_document
    event = json.loads((ROOT / f'local/assets/native/{code}/event-start.json').read_text())
    snapshot = Path(event['provenance']['snapshot']); snapshot = snapshot if snapshot.is_absolute() else ROOT / snapshot
    memory = zipfile.ZipFile(snapshot).read('eeMemory.bin')
    if hashlib.sha256(memory).hexdigest() != event['provenance']['ee_sha256']: raise ValueError('event-start.json was exported from another snapshot')
    doc = extract_document(memory, snapshot, event['participants'], code)
    for rider in doc['riders']:   # the provider steers on tick 0 (no countdown): it reads the retained previous lookahead
        rider['settings']['original_reset']['event_route']['previous_lookahead_point'] = rider['npc']['route_state']['previous_lookahead_point']
    doc['rolling_start'] = True
    doc['game_mode'] = event['game_mode']
    text = json.dumps(doc, indent=1, allow_nan=False) + '\n'
    for out in (ROOT / f'local/assets/native/{code}/npc-riders.json', ROOT / f'web/public/assets/{code}/npc-riders.json'):
        out.write_text(text)
    print(json.dumps(dict(riders=[(r['slot'], r['character']) for r in doc['riders']], anchor_tick=doc['anchor_tick']), indent=1))


# The rival documents per (game mode, human character): the peak rival 145750 is Mac, or Griff when the player is Mac
# (the web's Sam takes Mac's slot). Ready states: Zoe (tools/locations.py 'ready') and Mac, Rival Time and Rival Points.
RIVAL_STATES = {
    ('race', 'mac'): 'happiness-ready.p2s', ('race', 'griff'): 'happiness-mac-ready.p2s',
    ('jam', 'mac'): 'happiness-jam-ready.p2s', ('jam', 'griff'): 'happiness-jam-mac-ready.p2s',
}
# Other backcountry courses: their own ready states (Peak 3 The Throne: Psymon, or Elise when the player is Psymon;
# docs/peak3.md). A location without an entry here uses RIVAL_STATES (Happiness).
RIVAL_STATES_BY_LOCATION = {
    'EBC3': {('race', 'psymon'): 'the-throne-ready.p2s', ('race', 'elise'): 'the-throne-psymon-ready.p2s',
             ('jam', 'psymon'): 'the-throne-jam-ready.p2s', ('jam', 'elise'): 'the-throne-jam-psymon-ready.p2s'},
    # Peak 2 Ruthless: Nate, or Zoe when the player is Nate (docs/peak2.md; nate states from characters/nate/select.p2s)
    'DBC2': {('race', 'nate'): 'ruthless-ready.p2s', ('race', 'zoe'): 'ruthless-nate-ready.p2s',
             ('jam', 'nate'): 'ruthless-jam-ready.p2s', ('jam', 'zoe'): 'ruthless-jam-nate-ready.p2s'},
}


# The game RNG 0x4FF030 at the rolling start. A rival event has no countdown, so the ready state (race tick 0 not run) is
# the anchor: the rival's provider draws from these words on tick 0 (docs/backcountry.md "Rival computer rider"). Seeded 0
# (0x317958) before the load, then draws that depend on the human and on the menu path: the ready states hold 4 (Zoe on
# ABC1 / DBC2 / EBC3, Nate on DBC2, Psymon on EBC3) or 5 (Mac on ABC1). A player's direct path (Select Character -> Setup
# -> Select Peak -> event) follows the lineups' rule (tools/export_lineups.py: Moby +1, Zoe -1, a cheat skin +4) around a
# per-course base: Ruthless Zoe 3 / Nate 4 / Moby 5 (DIRECT_STATES, derived from characters/<human>/select.p2s through
# ruthless-nate-ready's menu script). The Peak 2 / Peak 3 Zoe states came from peak-1-selection with the pass patched,
# backing out of Select Peak and entering it again (docs/peak2.md, docs/peak3.md): one draw more (MENU_EXTRA; the lineups'
# `reference_anchor_extra` on Ruthless Ridge / Intimidator / Style Mile is the same path). web/lineup.js rivalAnchorWords:
# the document's own human starts from its ready state (the PS2 captures' start), any other human from the direct path.
RIVAL_LOAD_DRAWS = dict(character={'0': 1, '4': -1}, cheat=4)
MENU_EXTRA = {'ruthless-ready.p2s': 1, 'ruthless-jam-ready.p2s': 1, 'the-throne-ready.p2s': 1, 'the-throne-jam-ready.p2s': 1}
DIRECT_STATES = {'DBC2': {'zoe': 'local/ps2-capture/nav/p2/out-ruthless-zoe-direct/ready.p2s',
                          'moby': 'local/ps2-capture/nav/p2/out-ruthless-moby-direct/ready.p2s'}}


def anchor_rng(memory, extra=0):
    from export_lineups import seed_of
    words = list(struct.unpack_from('<6I', memory, 0x4FF030))
    seed, draws = seed_of(words, 100000)
    if seed != 0: raise ValueError(f'the game RNG is not seeded 0 at the load (seed {seed:#x})')
    return dict(seed=0, draws=draws, words=words, menu_extra=extra)


def rivals(code):
    """web/public/assets/<code>/rivals.json: {mode: {rival character: npc-riders document}} (docs/backcountry.md)."""
    from export_npc_riders import extract_document
    from locations import REFERENCE
    out = dict(version=1, location=code, load_draws=RIVAL_LOAD_DRAWS, documents={})
    for (mode, rival), name in RIVAL_STATES_BY_LOCATION.get(code, RIVAL_STATES).items():
        snapshot = REFERENCE / name
        if not snapshot.exists(): print(f'WARNING {code}: {name} missing; no {mode}/{rival} document'); continue
        memory = zipfile.ZipFile(snapshot).read('eeMemory.bin')
        race, participants = rolling_participants(memory)
        gm = game_mode(memory)
        if gm['mode'] != (4 if mode == 'race' else 5): raise ValueError(f'{name}: game mode {gm}')
        if [p['character'] for p in participants[1:]] != [rival]: raise ValueError(f'{name}: rival {[p["character"] for p in participants[1:]]}')
        doc = extract_document(memory, snapshot, participants, code, check_human=participants[0]['character'] == 'zoe')
        for rider in doc['riders']:
            rider['settings']['original_reset']['event_route']['previous_lookahead_point'] = rider['npc']['route_state']['previous_lookahead_point']
        doc.update(rolling_start=True, game_mode=gm, human=participants[0]['character'], human_start=dict(
            race=participants[0]['race'], ground=participants[0]['original_ground'], progress_origin=participants[0]['progress_origin']),
            anchor_rng=anchor_rng(memory, MENU_EXTRA.get(name, 0)))
        out['documents'].setdefault(mode, {})[rival] = doc
        print(mode, rival, 'human', participants[0]['character'], 'mode', gm, 'rank_mode', doc['world']['rank_mode'], 'variant', doc['event_variant'],
              'anchor draws', doc['anchor_rng']['draws'])
    # PS2 evidence for the direct path (race, Rival Time): the human's draws, checked against the documents' rule.
    for human, path in DIRECT_STATES.get(code, {}).items():
        state = ROOT / path
        if not state.exists(): print(f'WARNING {code}: {path} missing (direct-path evidence)'); continue
        memory = zipfile.ZipFile(state).read('eeMemory.bin')
        if game_mode(memory)['mode'] != 4: raise ValueError(f'{path}: not Rival Time')
        out.setdefault('direct_path', {})[human] = dict(draws=anchor_rng(memory)['draws'], state=path)
    (ROOT / f'web/public/assets/{code}/rivals.json').write_text(json.dumps(out, indent=None, allow_nan=False) + '\n')


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('command', choices=['event-start', 'npc', 'rivals']); p.add_argument('--location', default='ABC1')
    p.add_argument('--snapshot', help='event-start only: the ready state to export (default: the location\'s own)')
    p.add_argument('--output', help='event-start only: the event-start.json to write (default: local/assets/native/<code>/)')
    a = p.parse_args()
    if a.command == 'event-start':
        event_start(a.location, a.snapshot, a.output)
        return
    if a.snapshot or a.output:
        p.error('--snapshot / --output are for event-start')
    dict(**{'npc': npc, 'rivals': rivals})[a.command](a.location)


if __name__ == '__main__':
    main()
