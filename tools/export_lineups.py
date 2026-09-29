#!/usr/bin/env python3
"""Computer-rider lineups for every human rider (docs/characters.md "Computer-rider lineups").

The original picks the five computer riders of a race when the event loads (0x23A140 round manager ->
0x23A4F0 roster builder -> 0x23A668 copy -> 0x2342B8 setup slots). The riders depend on the human's
character, the peak rival and the roster RNG 0x4C9548 (seeded at the load by 0x22EFE8 with one draw of the
presentation RNG 0x4FF018). web/lineup.js runs that algorithm; this tool provides the data to build any
lineup it can produce.

1. `export`: for each derived countdown state local/reference/pcsx2/characters/<id>/countdown.p2s (and the
   reference anchor) the full npc-riders.json document of that grid (tools/export_npc_riders.py
   extract_document; participants from the race copy 0x535B20 instead of an assembly audit), saved to
   local/assets/native/<course>/lineups/<id>.json, plus the roster RNG facts of the state (seed, draws).
2. `build`: splits the observed computer-rider records into parts --
     slot parts (grid spot, route, start controller, race entry, ground state, pair records: the same for
     every character in a slot), character parts (settings, identity masks, grab catalog, ground profile,
     pair inputs: the same for a character in every slot and with every human), and the human-dependent
     parts (a cheat computer rider rides on the human's base character) -- and writes
     web/public/assets/<course>/lineups.json. It then assembles every observed lineup from the parts and
     requires the observed documents back exactly (addresses and provenance aside).
"""
import argparse, copy, hashlib, json, struct, sys, zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from export_npc_riders import extract_document, ROOT  # noqa: E402
from reference_race_event import extract_race_event  # noqa: E402
from reference_ground_profile import extract_ground  # noqa: E402

STATES = ROOT / 'local/reference/pcsx2/characters'
ROSTER = json.loads((ROOT / 'web/public/assets/riders.json').read_text())
NAME_OF = {(e['kind'] == 'cheat', e['character']): e['id'] for e in ROSTER if e['kind'] in ('rider', 'cheat')}
GP = 0x4A30F0
SEED_ADD = [0xF22D0E56, 0x96041893, 0x3DF3B646, 0x40DDE76D, 0x97327AE1, 0xD1A9FBE7]
M = 0xFFFFFFFF
FREESTYLE = {'ASS1', 'DSS2'}   # slope style Single Events: the human and one computer opponent (R&B, Style Mile)


# ---- the original generator 0x317A08 / seed 0x317958 ------------------------------------------------------------
def seeded(seed):
    words, a = [], seed & M
    for k in SEED_ADD: a = (a + k) & M; words.append(a)
    return words


def step_back(w):
    """Inverse of one 0x317A08 draw (the counter word w5 never wraps here)."""
    if w[5] == 0: raise ValueError('generator counter wrap')
    w5 = (w[5] - 1) & M
    w4 = (w[4] - w5) & M; c = 1 if (w[4] < w5 or w[4] < w4) else 0
    w3 = (w[3] - w[4] - c) & M; c = 1 if w[3] < w3 else 0
    w2 = (w[2] - w[3] - c) & M; c = 1 if w[2] < w2 else 0
    w1 = (w[1] - w[2] - c) & M; c = 1 if w[1] < w1 else 0
    w0 = (w[0] - w[1] - c) & M
    w[:] = [w0, w1, w2, w3, w4, w5]


def seed_of(words, limit=5_000_000):
    """(seed, draws since seeding) of a generator state."""
    w = list(words)
    for n in range(limit):
        s = (w[0] - SEED_ADD[0]) & M
        if seeded(s) == w: return s, n
        step_back(w)
    raise ValueError('no seeded state within the limit')


def rng_facts(memory):
    u = lambda a: struct.unpack_from('<I', memory, a)[0]
    roster_seed, roster_draws = seed_of([u(0x4C9548 + 4 * k) for k in range(6)])
    try: boot_seed, presentation_draws = seed_of([u(0x4FF018 + 4 * k) for k in range(6)])
    except ValueError: boot_seed = presentation_draws = None   # a poked presentation RNG (make_lineup_states.py)
    # which presentation draw produced the roster seed (0x22EFE8: 3177F0 -> 237CB0)
    draw = None
    if boot_seed is not None:
        w = seeded(boot_seed)
        for n in range(1, presentation_draws + 1):
            if next_word(w) == roster_seed: draw = n; break
    return dict(roster_seed=roster_seed, roster_draws=roster_draws, presentation_seed=boot_seed,
                presentation_draws=presentation_draws, roster_seed_draw=draw)


def next_word(w):
    v = (w[5] + w[4]) & M; c = 1 if (v < w[5] or v < w[4]) else 0; w[4] = v
    for k in (3, 2, 1):
        nv = (v + w[k] + c) & M; c = 1 if nv < w[k] else 0; v = nv; w[k] = v
    v = (v + w[0] + c) & M; w[0] = v; w[5] = (w[5] + 1) & M
    if w[5] == 0:
        k = 4
        while k >= 1:
            w[k] = (w[k] + 1) & M
            if w[k]: break
            k -= 1
        if k == 0: v = (v + 1) & M; w[0] = v
    return v


# ---- 0x23A4F0 (the browser's web/lineup.js is the same algorithm) -----------------------------------------------
def peak_rival(peak, human):
    """0x145750: the peak rival (course table 0x43D950 + event*100 + 0x54 = peak index)."""
    return {0: 5 if human == 3 else 3, 1: 4 if human == 7 else 7, 2: 6 if human == 8 else 8}.get(peak, 3)


def build_roster(seed, human, peak):
    w = seeded(seed)
    first = 10 + next_word(w) % 7
    r = next_word(w) % 6
    second = 10 + r if 10 + r < first else 11 + r
    entries, rival = [first, second], peak_rival(peak, human)
    for v in range(10):
        if len(entries) >= 10: break
        if v != human and v != rival: entries.append(v)
    entries += [0] * (10 - len(entries))
    for _ in range(25):   # 0x23C770: 25 swaps of two random entries
        i = next_word(w) % 10; j = next_word(w) % 10
        entries[i], entries[j] = entries[j], entries[i]
    return entries


def freestyle_roster(seed, human, rival):
    """0x239938 (R&B slope style): the characters but the human and the rival, 25 swaps of r % 8 (0x23C770)."""
    w = seeded(seed); e = [v for v in range(10) if v != human and v != rival]
    for _ in range(25):
        i = next_word(w) % len(e); j = next_word(w) % len(e); e[i], e[j] = e[j], e[i]
    return e


def riding_values(ls):
    """The computer riders' roster values (slots 1..): the race courses' five; R&B's one opponent (0x535BF8 riders)."""
    return [v or c for v, c in zip(ls['cheats'][1:], ls['characters'][1:])][:ls.get('riders', 6) - 1]


# ---- export ------------------------------------------------------------------------------------------------------
def memory_of(path):
    return zipfile.ZipFile(path).read('eeMemory.bin')


def participants_from_memory(memory):
    """event-start.json participants (tools/export_event_start.py) with the identity from the race copy
    0x535B20 (+0x11 gameplay character, +0x12 cheat id) instead of the model audit."""
    race = extract_race_event(memory)
    clock = race['clock']
    # six riders on the race courses; R&B slope style (ASS1): the human and the one computer opponent
    if clock['phase'] != 4 or clock['race_ticks'] != 0 or len(race['participants']) < 2: raise ValueError('not a countdown with computer riders')
    u = lambda at: struct.unpack_from('<I', memory, at)[0]
    game = int(race['provenance']['game_address'], 16)
    out = []
    for actor in race['participants']:
        slot = actor['index']; address = u(game + 0x28 + slot * 4)
        if actor['control_state'] != 6 or actor['motion_mode'] != 3 or any(actor['velocity']): raise ValueError('rider not held on the grid')
        owner = u(address + 0x77C); phase, steady, low, high, pose, rider = struct.unpack_from('<I4fI', memory, owner + 0x290)
        if rider != address or phase != 0: raise ValueError('unexpected start controller')
        entry = u(0x5305B0 + slot * 4)
        base, cheat = memory[0x535B20 + entry * 28 + 17], memory[0x535B20 + entry * 28 + 18]
        ground = extract_ground(memory, address)
        if ground['provenance']['character_id'] != base: raise ValueError('gameplay identity differs from the race copy')
        name = NAME_OF[(True, cheat)] if cheat else NAME_OF[(False, base)]
        out.append(dict(slot=slot, character=name, gameplay_character_id=base, render_package='RIDER_' + name.upper(), race=actor,
                        original_ground=ground, start_control=dict(phase=phase, steady_time=steady, low=low, high=high, pose=pose),
                        progress_origin=struct.unpack_from('<f', memory, address + 0x4D8)[0],
                        start_delay_seconds=struct.unpack_from('<i', memory, address + 0xB30)[0], reference_stance=u(address + 0x324)))
    return out


def roster_words(memory):
    u = lambda a: struct.unpack_from('<I', memory, a)[0]; i32 = lambda a: struct.unpack_from('<i', memory, a)[0]
    roster = u(GP - 0x480)
    shared_seed, shared_draws = seed_of([u(0x4FF030 + 4 * k) for k in range(6)], 100000)   # the game RNG, seeded 0 at the load
    return dict(characters=[i32(roster + 0x18 + 4 * k) for k in range(6)], cheats=[i32(roster + 0x40 + 4 * k) for k in range(6)],
                riders=u(0x535BF8), posted=[i32(0x536640 + 4 * k) for k in range(6)],
                event=i32(0x535C08), single_event=memory[0x535C11], tick=u(u(u(u(GP - 0x848) + 0x84) + 0x0C) + 8),
                shared_seed=shared_seed, shared_draws=shared_draws)


def state_paths(course):
    """Snow Jam: every character's derived countdown (characters/<id>) plus the coverage states with a chosen roster
    seed (characters/lineups, scripts/make_lineup_states.py); Metro-City: its coverage states (characters/lineups-BRA2); R&B and
    the Peak 2 courses likewise (characters/lineups-<course>: make_freestyle_states.py, make_peak2_lineup_states.py)."""
    out = {}
    # roster characters and '<skin>-on-<base>' compositions only (other evidence folders, e.g. mac-junction, are other events)
    ids = {e['id'] for e in ROSTER}
    if course == 'ARA1': out = {p.name: p / 'countdown.p2s' for p in sorted(STATES.iterdir()) if (p / 'countdown.p2s').exists() and (p.name in ids or '-on-' in p.name)}
    folder = STATES / ('lineups' if course == 'ARA1' else f'lineups-{course}')
    out.update({p.name: p / 'countdown.p2s' for p in sorted(folder.iterdir()) if (p / 'countdown.p2s').exists()})
    return out


def export(course):
    folder = ROOT / f'local/assets/native/{course}/lineups'; folder.mkdir(parents=True, exist_ok=True)
    anchor_event = json.loads((ROOT / f'local/assets/native/{course}/event-start.json').read_text())
    anchor = Path(anchor_event['provenance']['snapshot']); anchor = anchor if anchor.is_absolute() else ROOT / anchor
    items = [('__anchor', anchor)] + list(state_paths(course).items())
    summary = {}
    for name, path in items:
        memory = memory_of(path)
        participants = participants_from_memory(memory)
        if name == '__anchor':
            mine = json.dumps([{k: v for k, v in p.items()} for p in participants], sort_keys=True)
            if mine != json.dumps(anchor_event['participants'], sort_keys=True): raise ValueError('race-copy participants differ from event-start.json')
        doc = extract_document(memory, path, participants, course, check_human=participants[0]['character'] == 'zoe')
        facts = dict(rng_facts(memory), **roster_words(memory), human=participants[0]['character'],
                     human_base=participants[0]['gameplay_character_id'], lineup=[p['character'] for p in participants[1:]],
                     human_position=participants[0]['race']['position'], human_remaining=participants[0]['race']['remaining'],
                     human_ground_state=participants[0]['original_ground']['state'], human_body_scale=participants[0]['original_ground']['profile']['body_scale'])
        doc['lineup_state'] = facts
        (folder / f'{name}.json').write_text(json.dumps(doc, indent=1, allow_nan=False) + '\n')
        summary[name] = facts
        print(name, facts['lineup'], hex(facts['roster_seed']), facts['roster_draws'], facts['roster_seed_draw'], 'tick', facts['tick'], flush=True)
    (folder / 'summary.json').write_text(json.dumps(summary, indent=1) + '\n')


# ---- build: the parts ---------------------------------------------------------------------------------------------
# Leaf paths of a computer-rider record by what determines them, found from every observed record of the course (the
# first key that determines a leaf's value in all of them). Snow Jam: slot 27 leaves (route, AI path, NPC route words),
# grid 9 (ground frame, position, contact normal, lateral route distance: the spot moves ~90 cm x body scale along the
# grid line), skin 13 (scale, rig, masks, hair), base 13 (stance, uber table, ids: a cheat computer rider's are the
# HUMAN's base rider, 0x23A668), moment 15 (start controller -- diagnostic in the browser --, the 0xDF0 steering amount,
# previous ground normal, pose pivot, actor/cache addresses), state 5 (provenance words, the tick). Metro-City's flat
# grid keeps the ground frame constant and makes the route words follow the spot (grid 22, slot 16, moment 14).
CATEGORIES = [
    ('const', lambda o: 0), ('slot', lambda o: o['slot']), ('skin', lambda o: o['skin']), ('base', lambda o: o['base']),
    ('grid', lambda o: (o['slot'], o['scale'])), ('state', lambda o: o['state']), ('moment', lambda o: (o['state'], o['slot'])),
]


def classify(docs):
    obs = []
    for name, doc in docs.items():
        for r in doc['riders']:
            obs.append(dict(state=name, slot=r['slot'], scale=f32key(r['ground']['profile']['body_scale']), skin=r['character'],
                            base=r['gameplay_character_id'], flat=leaves(r)))
    shape = set(obs[0]['flat'])
    if any(set(o['flat']) != shape for o in obs): raise ValueError('records differ in shape')
    paths = {name: [] for name, _ in CATEGORIES}
    for path in sorted(shape):
        for name, key in CATEGORIES:
            seen = {}
            if all(seen.setdefault(key(o), json.dumps(o['flat'][path])) == json.dumps(o['flat'][path]) for o in obs):
                paths[name].append(path); break
        else: raise ValueError(f'{path}: not determined by slot, grid spot, skin, base or the state')
    return paths


def leaves(x, pre='', out=None):
    if out is None: out = {}
    if isinstance(x, dict):
        for k, v in x.items(): leaves(v, f'{pre}.{k}' if pre else k, out)
    else: out[pre] = x
    return out


def put(record, path, value):
    *head, last = path.split('.')
    for k in head: record = record[k]
    record[last] = value


def f32key(value):
    return struct.pack('<f', value)[::-1].hex()


def skin_name(value):
    return NAME_OF[(value >= 10, value)]


def peak_of(course):
    from export_career import Elf
    elf = Elf((ROOT / 'local/disc/SLUS_207.72').read_bytes())
    index = {'ARA1': 0, 'BRA2': 1, 'ASS1': 5, 'CRA3': 2, 'DRA4': 3, 'DSS2': 6}[course]   # Peak 2: Ruthless Ridge, Intimidator, Style Mile (docs/peak2.md)
    return struct.unpack('<i', elf.read(0x43D950 + index * 100 + 0x54, 4))[0]


def relationship_banks(memory):
    """Relationship records (0x155B50 reads the level byte +0xBC2): profile block 0x4A6CA8 + bank*0x9B50 + char*0xF88 +
    other*3 + 0xBC1..0xBC3 = [kind, level, score] as a hex triple, banks 0..2 (0 = the human's profile, 2 = computer)."""
    return [[[memory[0x4A6CA8 + bank * 0x9B50 + peer * 0xF88 + own * 3 + 0xBC1:0x4A6CA8 + bank * 0x9B50 + peer * 0xF88 + own * 3 + 0xBC4].hex()
              for own in range(10)] for peer in range(10)] for bank in range(3)]


# Relationship records (0x4A6CA8 + bank*0x9B50 + char*0xF88 + other*3 + 0xBC1): [kind, level, score].
# 0x155E58 (called by the event setup 0x233F20 / 0x2348AC, also on a restart) ages the records of every participant
# entry (bank 14A0E0, character 14A080): score - 3 (not below 0), level = score / 5, then the kind rules; a character
# raced twice in a bank ages twice. 0x155BF0 (in race, 107E70 -> 10E228/10E2E8/10E3A8/10E468) adds 1/2/4/6 for a soft
# bump / crash / soft attack / crash attack to the other rider's record about the one knocked, with the same rules.
FRESH_PROFILE = ROOT / 'local/reference/pcsx2/character-selection.p2s'


def _record(hexa):
    b = bytes.fromhex(hexa); return [x - 256 if x > 127 else x for x in b]


def _hex(kind, level, score):
    return bytes([kind & 255, level & 255, score & 255]).hex()


def relationship_age(rec):
    """0x155E58 on one record [kind, level, score]."""
    kind, level, score = rec
    score = max(score - 3, 0); level = int(score / 5)
    if kind == 1:
        if level >= 4: level, score, kind = 2, 10, 3
    elif kind == 0:
        if level >= 3: level, score = 2, 10
    elif kind == 2:
        if level <= 0: level = 1
        elif level >= 4: level, score = 3, 15
    elif kind == 3:
        if level == 0: kind, level, score = 1, 3, 15
        elif level >= 5: level, score = 4, 20
    return [kind, level, score]


def relationship_event(rec, event):
    """0x155BF0 on one record; event 0 soft, 1 crash, 2 soft attack, 3 crash attack."""
    kind, level, score = rec
    score += {0: 1, 1: 2, 2: 4, 3: 6}.get(event, 0); level = int(score / 5)
    if kind == 1:
        if level >= 4: level, score, kind = 2, 10, 3
    elif kind == 0:
        if level >= 3: level, score = 2, 10
    elif kind == 2:
        if level <= 0: level, score = 1, 5
        elif level >= 4: level, score = 3, 15
    elif kind == 3:
        if level <= 0: level, score = 1, 5
        elif level >= 5: level, score = 4, 20
    return [kind, level, score]


def event_relationships(before, characters, banks):
    table = copy.deepcopy(before)
    for c, bank in zip(characters, banks):
        table[bank][c] = [_hex(*relationship_age(_record(x))) for x in table[bank][c]]
    return table


def assemble(data, human_base, values, moment=None, state=None, extra=None):
    """The document for a lineup (web/lineup.js assembleLineup is the same). values: the five roster values (0x23A668)."""
    template = data['template']
    doc = copy.deepcopy({k: v for k, v in template.items() if k != 'riders'})
    riders = []
    for slot, v in enumerate(values, 1):
        base, skin = (v, skin_name(v)) if v < 10 else (human_base, skin_name(v))
        record = copy.deepcopy(template['riders'][slot - 1])
        grid = data['grid'][str(slot)][data['skin_scale'][skin]] if str(slot) in data['grid'] else {}   # R&B: one slot, the spot follows the skin
        parts = [data['slot'].get(str(slot), {}), grid, data['skin'][skin], data['base'].get(str(base), {}),
                 (moment or data['moment'])[str(slot)], (state or data['state'])[str(slot)]]
        for part in parts:
            for path, value in part.items(): put(record, path, copy.deepcopy(value))
        riders.append(record)
    doc['riders'] = riders
    characters = [human_base] + [v if v < 10 else human_base for v in values]
    count = len(characters)
    characters += template['relationships']['characters'][count:]   # R&B: the unused race slots' stale entries (0x5305B0; character 4 in every state)
    rel = doc['relationships']; banks = rel['target_banks']; rival = peak_rival(data['peak'], human_base)
    table = event_relationships(data['relationships_fresh'], characters[:count], banks)   # 0x155E58: the riders' records only
    level = lambda bank, peer, own: _record(table[bank][peer][own])[1]
    rel['scores'] = [[3 if own == rival else level(banks[peer], characters[peer], own) for peer in range(6)] for own in characters]
    rel['characters'] = characters; rel['persistent_human_character'] = human_base; rel['rival_character'] = rival; rel['event_kind'] = data['peak']
    world = doc['world']
    human_inputs = data.get('human_pair_inputs') or {}   # Peak 2: the human's own (attribute bank 0), see build
    world['pair_inputs'] = [copy.deepcopy(human_inputs.get(str(c)) if k == 0 and str(c) in human_inputs else data['pair_inputs'][str(c)]) for k, c in enumerate(characters[:count])]
    for a in range(len(world['records'])):   # +0x1C of record a->b (a < b): relationship >= 2 (10F560); distance/bearing: the first refresh (tick 0 = game tick 18)
        for b in range(6): world['records'][a][b]['t1c'] = int(a < b and world['records'][a][b]['enabled'] and rel['scores'][a][b] >= 2)
    if extra: doc.update(copy.deepcopy(extra))
    return doc


def build(course):
    folder = ROOT / f'local/assets/native/{course}/lineups'
    docs = {p.stem: json.loads(p.read_text()) for p in sorted(folder.glob('*.json')) if p.stem != 'summary'}
    anchor = docs['__anchor']
    PATHS = classify(docs)
    tables = {k: {} for k in PATHS}
    constants = {}

    def record(table, key, path, value, where):
        slot = table.setdefault(key, {})
        if path in slot and slot[path] != value: raise ValueError(f'{path} is not determined by {key} ({where})')
        slot[path] = value

    skin_scale = {}
    for name, doc in docs.items():
        human_base = doc['lineup_state']['human_base']
        for r in doc['riders']:
            flat = leaves(r); slot = str(r['slot']); skin = r['character']; base = str(r['gameplay_character_id'])
            scale = f32key(r['ground']['profile']['body_scale'])
            if skin_scale.setdefault(skin, scale) != scale: raise ValueError(f'{skin} scale differs')
            if r['character'] in {NAME_OF[(True, c)] for c in range(10, 30)} and r['gameplay_character_id'] != human_base: raise ValueError('cheat not on the human base')
            for path, value in flat.items():
                if path in PATHS['slot']: record(tables['slot'], slot, path, value, name)
                elif path in PATHS['grid']: record(tables['grid'].setdefault(slot, {}), scale, path, value, name)
                elif path in PATHS['skin']: record(tables['skin'], skin, path, value, name)
                elif path in PATHS['base']: record(tables['base'], base, path, value, name)
                elif path in PATHS['moment'] or path in PATHS['state']: pass
                elif path in PATHS['const']: record(constants, 'all', path, value, name)
                else: raise ValueError(path)
    moment = {str(r['slot']): {p: leaves(r)[p] for p in PATHS['moment']} for r in anchor['riders']}
    state = {str(r['slot']): {p: leaves(r)[p] for p in PATHS['state']} for r in anchor['riders']}
    fresh = relationship_banks(memory_of(FRESH_PROFILE))
    for name in docs:   # the relationship rule against the whole table of every state
        path = ROOT / docs[name]['provenance']['snapshot'] if name == '__anchor' else state_paths(course)[name]
        ls = docs[name]['lineup_state']
        characters = [ls['human_base']] + [c for c in ls['characters'][1:]]
        if course in FREESTYLE: characters = docs[name]['relationships']['characters'][:1 + len(docs[name]['riders'])]   # 0x155E58 ages the riders only
        if relationship_banks(memory_of(path)) != event_relationships(fresh, characters, docs[name]['relationships']['target_banks']):
            raise ValueError(f'{name}: relationship records differ from the load rule')
    pair_inputs, by_role, role_split = {}, ({}, {}), False
    for name, doc in docs.items():
        for s, c in enumerate(doc['relationships']['characters'][:len(doc['world']['pair_inputs'])]):
            value = doc['world']['pair_inputs'][s]
            if by_role[s > 0].setdefault(str(c), value) != value: raise ValueError('pair inputs differ per character')
            if pair_inputs.setdefault(str(c), value) != value: role_split = True
    # Peak 2: the computer riders' collision / attack stats come from attribute bank 2 (raw 20, docs/peak2.md), the human's
    # from its own bank: pair inputs by character for the computer riders, human_pair_inputs for slot 0.
    human_pair_inputs = None
    if role_split: pair_inputs, human_pair_inputs = by_role[1], by_role[0]
    # The game RNG 0x4FF030 at the countdown anchor: seeded 0 at the load, the load's draws (depend only on the human
    # character and the course), then the five computer riders' start-command draws (0x10B.. originalNpcStartCommand at the
    # first countdown pass: two each). Fit on the base riders and one cheat, checked on every state.
    cheat_ids = {NAME_OF[(True, c)] for c in range(10, 30)}
    start = 2 * len(anchor['riders'])   # two start-command draws per computer rider
    def fit(names):
        seen = {}
        for name in names:
            ls = docs[name]['lineup_state']; n = ls['shared_draws'] - start
            if ls['shared_seed'] != 0: raise ValueError(f'{name}: the game RNG is not seeded 0 at the load')
            seen.setdefault(('cheat' if ls['human'] in cheat_ids else 'rider', ls['human_base']), set()).add(n)
        return seen
    seen, anchor_extra = fit(docs), 0
    if any(len(v) != 1 for v in seen.values()):
        # Peak 2: the reference anchor (menu path peak-1-selection -> Peak 2 pass -> Setup re-entered, docs/peak2.md) makes one
        # load draw more than the same Zoe lineup reached from Select Character (characters/lineups-CRA3/zoe-b57109a9, the
        # path every coverage state and a player takes). Fit the rule without the anchor and keep its offset.
        seen = fit([n for n in docs if n != '__anchor'])
        if any(len(v) != 1 for v in seen.values()): raise ValueError(f'load draws depend on more than the human: {seen}')
        ls = anchor['lineup_state']; hb = ls['human_base']
        anchor_extra = ls['shared_draws'] - start - next(iter(seen[('cheat' if ls['human'] in cheat_ids else 'rider', hb)]))
    riders = {b: v for (k, b), v in seen.items() if k == 'rider'}
    common = {b: next(iter(v)) for b, v in riders.items()}
    values = sorted(common.values()); base = max(set(values), key=values.count)
    load_draws = dict(base=base, character={str(b): n - base for b, n in common.items() if n != base}, cheat=4, start=start)
    if anchor_extra: load_draws['reference_anchor_extra'] = anchor_extra   # diagnostic: web/lineup.js loadDraws uses the rule
    for name, doc in docs.items():
        ls = doc['lineup_state']
        want = start + base + load_draws['character'].get(str(ls['human_base']), 0) + (4 if ls['human'] in cheat_ids else 0) + (anchor_extra if name == '__anchor' else 0)
        if ls['shared_draws'] != want: raise ValueError(f'{name}: {ls["shared_draws"]} game RNG draws at the anchor, the rule says {want}')
    # The human's own grid spot on this course (slot 0): the ground state follows the body scale, reverse_stance the base
    # rider; every other leaf is the same for every human. (Snow Jam's per-character spots are also in RIDER_<ID>/settings.json.)
    hobs = [(d['lineup_state'], leaves(d['lineup_state']['human_ground_state'])) for d in docs.values()]
    human_grid, human_base, human_const = {}, {}, {}
    for ls, flat in hobs:
        key = f32key(ls['human_body_scale'])
        for path, value in flat.items():
            table = human_base.setdefault(str(ls['human_base']), {}) if path == 'reverse_stance' else human_grid.setdefault(key, {})
            if path in table and table[path] != value: raise ValueError(f'human grid {path} is not determined by the body scale / base rider')
            table[path] = value
    human_scale = {}
    for e in ROSTER:
        f = ROOT / f'web/public/assets/{e["package"]}/settings.json'
        doc = json.loads(f.read_text()) if f.exists() else None
        scale = (doc or {}).get('settings', {}).get('original_event_start', {}).get('body_scale')
        human_scale[e['id']] = f32key(scale) if scale is not None else f32key(anchor['lineup_state']['human_body_scale'])   # Zoe
    # Presentation draws after the roster seed (0x4FF018 per-frame effects of the load, overlay, countdown and race): the
    # measured average of the reference sessions (web/lineup.js noteEventDraws; not reproducible draw by draw).
    after = [d['lineup_state']['presentation_draws'] - d['lineup_state']['roster_seed_draw'] - 18 * 0 for d in docs.values()
             if d['lineup_state'].get('roster_seed_draw') and d['lineup_state']['tick'] == 18]
    rates = []
    for cid in sorted(p.name for p in STATES.iterdir() if (p / 'glide.p2s').exists() and (p / 'countdown.p2s').exists()):
        c, g = memory_of(STATES / cid / 'countdown.p2s'), memory_of(STATES / cid / 'glide.p2s')
        uc = lambda m, a: struct.unpack_from('<I', m, a)[0]
        (_, nc), (_, ng) = seed_of([uc(c, 0x4FF018 + 4 * k) for k in range(6)]), seed_of([uc(g, 0x4FF018 + 4 * k) for k in range(6)])
        tc, tg = roster_words(c)['tick'], roster_words(g)['tick']
        rates.append((ng - nc) / (tg - tc))
    if not after:   # no reference-session countdown at tick 18 here (R&B's anchor is at 19): Snow Jam's model
        presentation_model = json.loads((ROOT / 'web/public/assets/ARA1/lineups.json').read_text())['presentation_model']
    else: presentation_model = dict(anchor_tick=18, after_seed=round(sum(after) / len(after)), per_tick=round(sum(rates) / len(rates), 3),
                              after_seed_range=[min(after), max(after)], per_tick_range=[round(min(rates), 3), round(max(rates), 3)], samples=[len(after), len(rates)])
    template = {k: v for k, v in anchor.items() if k != 'lineup_state'}
    data = dict(version=1, course=course, peak=peak_of(course),
                names=dict(base=[NAME_OF[(False, c)] for c in range(10)], cheat={str(c): NAME_OF[(True, c)] for c in range(10, 30)}),
                anchor=dict(human=anchor['lineup_state']['human'], human_base=anchor['lineup_state']['human_base'], seed=anchor['lineup_state']['roster_seed'],
                            values=riding_values(anchor['lineup_state'])),
                paths={k: v for k, v in PATHS.items() if k != 'const'}, slot=tables['slot'], grid=tables['grid'], skin_scale=skin_scale, skin=tables['skin'], base=tables['base'],
                moment=moment, state=state, relationships_fresh=fresh, load_draws=load_draws,
                human_template=anchor['lineup_state']['human_ground_state'], presentation_model=presentation_model, human_grid=human_grid, human_base=human_base, human_scale=human_scale, pair_inputs=pair_inputs, template=template,
                coverage=dict(states=len(docs), grid={s: sorted(v) for s, v in tables['grid'].items()}, skins=sorted(tables['skin']), bases=sorted(tables['base'])),
                # every observed countdown: the roster seed, the human and the five roster values (web/test-lineups.mjs)
                observed=[dict(state=name, human=d['lineup_state']['human'], human_base=d['lineup_state']['human_base'], seed=d['lineup_state']['roster_seed'],
                               values=riding_values(d['lineup_state']), gmm_characters=d['lineup_state']['characters'], posted=d['lineup_state'].get('posted'),
                               tick=d['lineup_state']['tick'], human_position=d['lineup_state'].get('human_position'), shared_draws=d['lineup_state']['shared_draws'],
                               presentation_seed=d['lineup_state']['presentation_seed'], seed_draw=d['lineup_state']['roster_seed_draw'])
                          for name, d in docs.items()],
                provenance=dict(tool='tools/export_lineups.py', states='local/assets/native/%s/lineups/*.json (export)' % course))
    if human_pair_inputs is not None: data['human_pair_inputs'] = human_pair_inputs
    # every observed document back from the parts (with its own moment/snapshot words), world.records distance/bearing aside
    # (the 10F560 refresh recomputes them from the grid positions at the first tick; web/test-lineups.mjs checks that in the core)
    def strip(doc):
        doc = copy.deepcopy(doc); doc.pop('lineup_state', None); doc.pop('provenance', None)
        for row in doc['world']['records']:
            for r in row: r.pop('distance'); r.pop('bearing')
        return doc
    for name, doc in docs.items():
        ls = doc['lineup_state']
        values = riding_values(ls)
        own_moment = {str(r['slot']): {p: leaves(r)[p] for p in PATHS['moment']} for r in doc['riders']}
        own_state = {str(r['slot']): {p: leaves(r)[p] for p in PATHS['state']} for r in doc['riders']}
        got = assemble(data, ls['human_base'], values, own_moment, own_state, dict(anchor_tick=doc['anchor_tick'], human_identity=doc['human_identity']))
        if json.dumps(strip(got)) != json.dumps(strip(doc)):
            a, b = leaves(strip(got)), leaves(strip(doc))
            diff = [k for k in set(a) | set(b) if a.get(k) != b.get(k)]
            raise ValueError(f'{name}: the parts do not reproduce the document: {diff[:8]} {[(a.get(k), b.get(k)) for k in diff[:3]]}')
        if course in FREESTYLE:   # 0x239938: a Single Event rides the last shuffled character, slots 2..5 post entries 0..3
            entries = freestyle_roster(ls['roster_seed'], ls['human_base'], peak_rival(data['peak'], ls['human_base']))
            if [entries[-1]] != values or entries[:4] != ls['characters'][2:6]: raise ValueError(f'{name}: 0x239938 does not give the observed roster')
        elif build_roster(ls['roster_seed'], ls['human_base'], data['peak'])[:5] != values: raise ValueError(f'{name}: 0x23A4F0 does not give the observed lineup')
    grid_missing = [(s, k) for s in map(str, range(1, 1 + len(anchor['riders']))) for k in {v for v in skin_scale.values()} if k not in tables['grid'].get(s, {})]
    out = ROOT / f'web/public/assets/{course}/lineups.json'
    out.write_text(json.dumps(data, separators=(',', ':'), allow_nan=False))
    print(json.dumps(dict(output=str(out.relative_to(ROOT)), bytes=out.stat().st_size, states=len(docs), skins=len(tables['skin']), bases=sorted(tables['base']),
                          grid={s: len(v) for s, v in tables['grid'].items()}, grid_missing=grid_missing, reproduced='all'), indent=1))


def sessions():
    """Two events / a restart in one session (characters/two-event, zoe countdown -> race T frames -> pause > Quit or
    Restart -> the next load): relationship tables, participants and presentation draws for web/test-lineups.mjs."""
    folder = STATES / 'two-event'; out = {}
    fresh = relationship_banks(memory_of(FRESH_PROFILE))
    for sess in sorted(p for p in folder.iterdir() if p.is_dir()):
        row = {}
        for st in ('race1', 'select2', 'restart-b', 'ready2', 'countdown2'):
            f = sess / f'{st}.p2s'
            if not f.exists(): continue
            m = memory_of(f); u = lambda a: struct.unpack_from('<I', m, a)[0]
            item = dict(relationships=relationship_banks(m), presentation_draws=seed_of([u(0x4FF018 + 4 * k) for k in range(6)])[1],
                        roster_seed=seed_of([u(0x4C9548 + 4 * k) for k in range(6)])[0])
            try:
                rw = roster_words(m); item.update(tick=rw['tick'], characters=rw['characters'], cheats=rw['cheats'], shared_draws=rw['shared_draws'])
                entries = [u(0x5305B0 + 4 * k) for k in range(6)]
                item['banks'] = [2 if struct.unpack_from('<i', m, 0x535B20 + e * 28 + 12)[0] == -1 else u(0x535B20 + e * 28 + 16) & 1 for e in entries]
            except Exception: pass
            row[st] = item
        out[sess.name] = row
    target = ROOT / 'web/public/assets/ARA1/lineup-sessions.json'
    target.write_text(json.dumps(dict(boot_seed=0x182200, fresh=fresh, sessions=out, provenance='tools/export_lineups.py sessions: local/reference/pcsx2/characters/two-event'), separators=(',', ':')))
    print({k: {st: (v.get('tick'), v['presentation_draws'], hex(v['roster_seed'])) for st, v in r.items()} for k, r in out.items()})


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('action', choices=['export', 'build', 'sessions'])
    parser.add_argument('--course', default='ARA1')
    a = parser.parse_args()
    if a.action == 'export': export(a.course)
    elif a.action == 'sessions': sessions()
    else: build(a.course)


if __name__ == '__main__':
    main()
