#!/usr/bin/env python3
"""Export the five original Snow Jam computer riders at the countdown grid (browser AI racers).

Everything comes from the owned countdown-anchor savestate (the base of the event PS2 captures),
through the same verified extractors the human's browser settings use. For each computer rider:

* `settings`: overrides of web/public/assets/ANIMATIONS/initial.json sections (the human settings
  document) with this rider's own values: animation inputs (scale = body scale, contact legs,
  secondary motion slot/hair), landing/boost/air-entry/grab/trick/rail/reset profiles. The human's
  sections extracted the same way from the same snapshot equal initial.json (checked below), so a
  difference is a real per-character difference (body scale, uber grab table, stance, hair slot).
* `ground`: event-start.json's original ground profile/state for the grid (as the human's seed).
* `npc`: provider 0x10A768 state (owner DF0..F40 typed fields, route, pacing, grab catalog) from
  tools/reference_npc.py, plus identity fields the browser rider pipeline branches on.

No recorded future state is exported; the browser runs every tick natively.
"""
import hashlib, json, struct, zipfile
from pathlib import Path
from reference_landing import extract_landing
from reference_boost import extract_boost
from reference_air_entry import extract_air_entry
from reference_rail_context import extract_memory as extract_rail
from reference_grab_lifecycle import extract_grab_lifecycle
from reference_trick_identity import extract_trick_identity
from reference_reset import extract_reset
from reference_npc import extract_npcs
from probe_rider_pose import animation_inputs
from reference_pair_collision import extract_pair_collision
from reference_secondary_motion import extract_secondary_motion

ROOT = Path(__file__).resolve().parents[1]


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--location', default='ARA1', help='course code with a six-rider event-start.json (ARA1 Snow Jam, BRA2 Metro-City)')
    location = parser.parse_args().location
    event = json.loads((ROOT / f'local/assets/native/{location}/event-start.json').read_text())
    snapshot = Path(event['provenance']['snapshot'])
    if not snapshot.is_absolute(): snapshot = ROOT / snapshot
    memory = zipfile.ZipFile(snapshot).read('eeMemory.bin')
    digest = hashlib.sha256(memory).hexdigest()
    if event['provenance']['ee_sha256'] != digest:
        raise ValueError('event-start.json was exported from another snapshot')
    result = extract_document(memory, snapshot, event['participants'], location)
    text = json.dumps(result, indent=1, allow_nan=False) + '\n'
    for out in (ROOT / f'local/assets/native/{location}/npc-riders.json', ROOT / f'web/public/assets/{location}/npc-riders.json'):
        out.write_text(text)
    print(json.dumps(dict(riders=[(r['slot'], r['character'], r['identity']) for r in result['riders']], human=result['human_identity']), indent=1))


def rig_folder(package):
    """Native rig of a render package (tools/export_characters.py native_folder: the five Snow Jam
    opponents and Zoe in local/assets/native, the other characters in local/assets/native/CHARACTERS)."""
    # CHARACTERS first: local/assets/native/RIDER_MAC is rider_assets.py's Mac test outfit (the sam_mesh source), while
    # CHARACTERS/RIDER_MAC is the live Mac assembly (the Happiness rival, docs/backcountry.md); no other package is in both.
    character = ROOT / 'local/assets/native/CHARACTERS' / package
    if (character / 'rider.json').exists(): return character
    return ROOT / 'local/assets/native' / package


def extract_document(memory, snapshot, participants, location, check_human=True):
    """The npc-riders.json document of a countdown snapshot. `participants`: event-start.json style
    entries (slot, character, render_package, gameplay_character_id, race, progress_origin, start_control,
    reference_stance, start_delay_seconds, original_ground). check_human: the human's extraction must equal
    initial.json (true for a Zoe human; tools/export_lineups.py passes False for the other humans)."""
    digest = hashlib.sha256(memory).hexdigest()
    initial = json.loads((ROOT / 'web/public/assets/ANIMATIONS/initial.json').read_text())
    u = lambda at: struct.unpack_from('<I', memory, at)[0]
    i32 = lambda at: struct.unpack_from('<i', memory, at)[0]
    f = lambda at: struct.unpack_from('<f', memory, at)[0]
    q = lambda at: struct.unpack_from('<Q', memory, at)[0]
    game = u(u(u(0x4A30F0 - 0x848) + 0x84) + 0x0C)
    HUMAN = u(game + 0x28)  # roster slot 0 (0x14701A0 in the Snow Jam anchor)

    def profiles(actor):
        return dict(original_landing=extract_landing(memory, actor), original_boost=extract_boost(memory, actor),
                    original_air_entry=extract_air_entry(memory, actor), original_rail_context=extract_rail(memory, actor),
                    original_grab_control=extract_grab_lifecycle(memory, actor), original_trick_identity=extract_trick_identity(memory, actor))

    # Extractor semantics check: the human's character sections from this snapshot equal the shipped settings.
    human = profiles(HUMAN)
    if check_human and json.dumps(extract_secondary_motion(memory, HUMAN), sort_keys=True) != json.dumps(initial['original_animation']['secondary_motion'], sort_keys=True):
        raise ValueError('Human secondary motion extracted from the anchor differs from initial.json')
    for name, keys in [('original_landing', ['profile']), ('original_boost', ['profile', 'award_context']), ('original_air_entry', ['profile']),
                       ('original_grab_control', ['profile']), ('original_trick_identity', ['profile', 'commit_profile', 'named_tricks', 'rider_stance'])] if check_human else []:
        for key in keys:
            if json.dumps(human[name][key], sort_keys=True) != json.dumps(initial[name][key], sort_keys=True):
                raise ValueError(f'Human {name}.{key} extracted from the anchor differs from initial.json')

    npcs = extract_npcs(memory)
    routes = {r['slot']: r for r in npcs['participant_routes']}
    riders = []
    for participant in participants[1:]:
        slot = participant['slot']
        actor = u(game + 0x28 + slot * 4)
        npc = next(r for r in npcs['riders'] if r['slot'] == slot)
        if int(npc['rider'], 16) != actor:
            raise ValueError('Roster order differs')
        package = participant['render_package']
        rig = rig_folder(package)
        settings = profiles(actor)
        animation = animation_inputs(snapshot, actor, rig_path=rig)
        settings['original_animation'] = {k: animation[k] for k in ('scale', 'contact', 'pivot_bone', 'default_root_position', 'default_root_rotation',
                                                                    'default_mirror', 'bone_mask', 'limited_bones', 'variant_flags', 'secondary_motion', 'character')
                                          if k in animation}
        settings['original_animation']['secondary_motion'] = extract_secondary_motion(memory, actor)
        reset = extract_reset(memory, actor)
        reset.pop('paths')  # the AI path bank is shared; initial.json already carries it
        route = routes[slot]
        reset['event_route'] = dict(path_index=route['path_index'], cache=route['cache'], closest_point=route['closest_point'],
                                    lookahead_point=route['lookahead_point'], previous_distance=route['previous_distance'],
                                    current_distance=route['current_distance'], lateral_distance=route['lateral_distance'], heading=route['heading'])
        settings['original_reset'] = reset
        for key in ('diagnostic_ai_words', 'accepted_command_history', 'ground'):
            npc.pop(key, None)
        owner = u(actor + 0x77C)
        identity = dict(human874=u(actor + 0x874), device870=i32(actor + 0x870), device_enabled87c=u(actor + 0x87C),
                        contact_cache864=i32(actor + 0x864), body_cache868=i32(actor + 0x868), category_b20=i32(actor + 0xB20),
                        capability_b2c=u(actor + 0xB2C), limited_bones_b1c=u(actor + 0xB1C), progress_inhibit_ac4=u(actor + 0xAC4),
                        rider_type434=i32(actor + 0x434), time_scale300=f(actor + 0x300), super_time2f0=f(actor + 0x2F0),
                        boost_meter2f8=f(actor + 0x2F8), designated_f0=u(actor + 0xF0), designated_slot_f8=i32(actor + 0xF8),
                        upper_mask8c0=hex(q(actor + 0x8C0)), upper_mask8c8=hex(q(actor + 0x8C8)), upper_mask8d0=hex(q(actor + 0x8D0)),
                        behavior_f48=hex(u(owner + 0xF48)), owner=hex(owner))
        # Stat getters 0x1494C0.. (web/rider_attributes.hpp): the progress bytes of runtime attribute bank 0x535538 row
        # 14A0E0(entry) (race copy 0x535B20 +0xC == -1 -> bank 2, else +0x10 & 1), character 14A080 (+0x11). Computer riders
        # use bank 2; on Peak 2 it holds 20 (stat 4/11) for them against the human's 5 (docs/peak2.md).
        entry = u(0x5305B0 + slot * 4); race_copy = 0x535B20 + entry * 28
        bank = 2 if i32(race_copy + 0xC) == -1 else u(race_copy + 0x10) & 1; char = memory[race_copy + 0x11]
        attributes = dict(bank=bank, character=char, raw=list(memory[0x535538 + bank * 70 + char * 7:0x535538 + bank * 70 + char * 7 + 7]),
                          override=i32(actor + 0xB34))
        # emitted only where it differs from the level-1 seed every browser core starts with (Peak 1 anchors: raw 5, no override)
        extra = dict(attributes=attributes) if attributes['raw'] != [5] * 7 or attributes['override'] else {}
        riders.append(dict(slot=slot, actor=hex(actor), character=participant['character'], package=package, **extra,
                           gameplay_character_id=participant['gameplay_character_id'], race=participant['race'],
                           progress_origin=participant['progress_origin'], start_control=participant['start_control'],
                           reference_stance=participant['reference_stance'], start_delay_seconds=participant['start_delay_seconds'],
                           ground=participant['original_ground'], settings=settings, npc=npc, identity=identity))
    if check_human and (q(HUMAN + 0x8C0), q(HUMAN + 0x8C8), q(HUMAN + 0x8D0)) != (0x8000fffe, 0x8000fff8, 0x870):
        raise ValueError('Human channel-1 masks differ from the browser defaults (web/animation_bridge.cpp riderMask8C0..)')
    human_identity = dict(human874=u(HUMAN + 0x874), body_cache868=i32(HUMAN + 0x868), category_b20=i32(HUMAN + 0xB20),
                          capability_b2c=u(HUMAN + 0xB2C), time_scale300=f(HUMAN + 0x300))
    # Game-info +8 at the anchor: the grid seeds are this tick's state (the browser's event clock starts at
    # tick 0 and reaches it with the riders frozen; computer riders replay their retained grid command).
    anchor_tick = u(game + 8)
    world = dict(count=u(game + 0x78), tail=u(game + 0x84), rank_mode=u(game + 0x74),
                 ranks=[i32(u(game + 0x28 + s * 4) + 0xEC) for s in range(u(game + 0x78))],
                 records=[[dict(enabled=u(r + b * 0x24), human=u(r + b * 0x24 + 4), distance=f(r + b * 0x24 + 8), bearing=f(r + b * 0x24 + 12),
                                  t10=i32(r + b * 0x24 + 0x10), t14=i32(r + b * 0x24 + 0x14), t18=i32(r + b * 0x24 + 0x18), t1c=i32(r + b * 0x24 + 0x1C), t20=i32(r + b * 0x24 + 0x20))
                              for b in range(6)] for r in [u(game + 0x28 + a * 4) for a in range(u(game + 0x78))]])
    pairs = extract_pair_collision(memory)
    world['pair_inputs'] = [dict(weight_attribute=p['weight_attribute'], collision_stat=p['resolved_collision_stat'], attack_stat=p['resolved_attack_stat']) for p in pairs['participants']]
    world['knockdown_cheat'] = bool(pairs['knockdown_cheat'])
    result = dict(version=1, location=location, anchor_tick=anchor_tick, world=world, event_variant=npcs['event_variant'], relationships=npcs['relationships'],
                  human_identity=human_identity, riders=riders,
                  provenance=dict(snapshot=str(snapshot.relative_to(ROOT)), ee_sha256=digest,
                                  role='Initial computer-rider state at the countdown grid; every later tick runs natively'))
    return result


if __name__ == '__main__':
    main()
