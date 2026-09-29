#!/usr/bin/env python3
"""The step list of tools/setup_from_iso.py, in dependency order (docs/iso-pipeline.md "Steps").

Each step is one tool invocation, run from the checkout root. `needs`: iso, gamecube, states (the state pack or, for
maintainers, the real savestates), sam, movies. Placeholders: {py} the Python running the driver, {iso}, {gamecube},
{pack}. Groups (step name prefixes):

  disc-*       extraction and checks                 shared-*   animation banks, textures, FX, irradiance
  loc-<L>-*    one course/event location             post-<L>-* its set pieces, sections, opponents
  riders-*     rider packages, roster, wardrobe      ui-*, career-*, cut-*, audio-*, movies
  peak<N>-*    the streamed peaks                    mountain-* the whole mountain
  core-*       generated code and the WASM core      final-*    course manifest, texture archives

SPDX-License-Identifier: GPL-3.0
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from locations import LOCATIONS  # noqa: E402

PY = '{py}'
RIDERS = ('zoe', 'psymon', 'allegra', 'moby', 'griff', 'luther')
# The Snow Jam event keeps its historical order first; the others follow tools/locations.py.
ORDER = list(LOCATIONS)
# Peak free-ride / whole-mountain seeds: the savestate each was exported from (recorded in PEAK*/seed-state.json).
PEAK_SEEDS = {
    1: ('local/ps2-capture/peak1/fr-aara1-glide.p2s', 'A'),
    2: ('local/ps2-capture/peak2/frd-1800.p2s', 'D'),
    3: ('local/ps2-capture/peak3/derived/fr-ebc3-14302.p2s', 'EBC3'),
}
# Inputs that no pipeline step produces and that come from the state pack (kept whole). Each is the output of a
# maintainer-only tool: an emulator or recompiled-code oracle, or a native engine build (docs/iso-pipeline.md).
EVIDENCE = {
    **{f'local/assets/native/RIDER_{r.upper()}/animation-start.json':
       'tools/test_opponent_poses.py --export-initialization (needs the native Metal engine audit binary); test-only data'
       for r in ('psymon', 'allegra', 'moby', 'griff', 'luther')},
}
MOUNTAIN_SEEDS = {
    'MOUNTAIN': ('local/ps2-capture/allpeak/nav/out-apr-card/after.p2s', 'EBC3'),
    'MOUNTAIN2': ('local/ps2-capture/allpeak/nav/out-p2r-card/after.p2s', 'DBC2'),
    'MOUNTAINF': ('local/ps2-capture/ctm-parity/mountain/states/frdra4-lodgeD.p2s', 'DRA4'),
    'MOUNTAINJ': ('local/ps2-capture/allpeak/nav/out-apj-card/after.p2s', 'EBC3'),
}


def registry(Step):
    S = Step
    out = []
    add = out.append

    # ---------------------------------------------------------------------------------------------- disc
    add(S('disc-ps2', [PY, 'tools/extract_disc.py', '--iso', '{iso}'], needs=['iso']))
    add(S('disc-gamecube', [PY, 'tools/extract_gamecube.py', '{gamecube}'], needs=['gamecube']))
    add(S('disc-statepack', [PY, 'tools/statepack.py', 'restore', '{pack}'], needs=['pack']))

    # ---------------------------------------------------------------------------------------------- shared
    add(S('shared-animation-bank', [PY, 'tools/animation_bank.py', '--source', 'ps2'], needs=['iso', 'gamecube']))
    add(S('shared-animation-library', [PY, 'tools/export_animation_library.py']))
    add(S('shared-irradiance', [PY, 'tools/export_irradiance.py'], needs=['gamecube']))
    add(S('shared-world-textures', [PY, 'tools/export_world_textures.py'], needs=['gamecube']))
    add(S('shared-board-trail', [PY, 'tools/import_board_trail.py', '--iso', '{iso}'], needs=['iso']))
    add(S('shared-impact-fx', [PY, 'tools/export_impact_fx.py'], needs=['iso']))
    add(S('shared-fx-textures', [PY, 'tools/export_fx_textures.py'], needs=['iso']))
    add(S('shared-snow-assets', [PY, 'tools/export_snow_assets.py'], needs=['iso', 'gamecube', 'states']))

    # ---------------------------------------------------------------------------------------------- riders (native)
    # Zoe (the human's rider in Snow Jam) and the five Snow Jam opponents: GameCube models, PS2 animation bank.
    for r in RIDERS:
        add(S(f'riders-{r}-model', [PY, 'tools/rider_assets.py', '--rider', r], needs=['gamecube']))
        add(S(f'riders-{r}-samples', [PY, 'tools/export_animation_samples.py', '--rider', r, '--source', 'ps2'], needs=['states']))

    # ---------------------------------------------------------------------------------------------- locations
    for L in ORDER:
        steps = location_steps(S, L)
        if L == 'ARA1':
            # after Snow Jam's first web package (RIDER_ZOE, SNOW_FX): tools that update the native and web copies
            at = next(i for i, s in enumerate(steps) if s.name == 'loc-ARA1-web-1') + 1
            steps[at:at] = [
                S('riders-zoe-skin', [PY, 'tools/export_rider_skin_weights.py', '--rider', 'zoe'], needs=['gamecube']),
                S('riders-zoe-bind', [PY, 'tools/export_rider_bind_matrices.py'], needs=['states']),
                S('shared-snow-flipbooks', [PY, 'tools/export_snow_flipbooks.py'], needs=['gamecube']),
                S('shared-snow-view', [PY, 'tools/export_snow_view.py'], needs=['states']),
            ]
        out.extend(steps)
    add(S('stage-scripts', [PY, 'tools/export_stage_scripts.py'], needs=['states']))
    add(S('startfire', [PY, 'tools/export_startfire.py'], needs=['iso']))
    for L in ORDER:
        out.extend(post_steps(S, L))
    return out


def location_steps(S, L):
    """One location up to its web package (tools/prepare_location.py order; Snow Jam's historical paths)."""
    info = LOCATIONS[L]
    ev = info['event']
    st = lambda kind: 'local/reference/pcsx2/' + info['states'][kind]
    p = f'loc-{L}-'
    steps = [
        S(p + 'import-world', [PY, 'tools/import_world.py', '--location', L], needs=['gamecube']),
        S(p + 'import-sky', [PY, 'tools/import_sky.py', '--location', L]),
        S(p + 'import-rails', [PY, 'tools/import_rails.py', '--locations', L]),
        S(p + 'environment-lighting', [PY, 'tools/export_environment_lighting.py', '--location', L]),
        S(p + 'local-lights', [PY, 'tools/export_local_lights.py', '--location', L]),
        S(p + 'stage-scripts', [PY, 'tools/import_stage_scripts.py', '--location', L]),
        S(p + 'stage-disassembly', [PY, 'tools/disassemble_stage_scripts.py', '--location', L]),
        S(p + 'web-1', [PY, 'web/prepare.py', '--location', L]),
        S(p + 'fog-tree', [PY, 'tools/export_fog_tree.py', '--location', L]),
        S(p + 'terrain-render', [PY, 'tools/prepare_terrain_render.py', '--location', L]),
        S(p + 'sun-flare', [PY, 'tools/export_sun_flare.py', '--location', L], needs=['iso']),
        S(p + 'light-glow', [PY, 'tools/export_light_glow.py', '--location', L], needs=['iso']),
        S(p + 'glare', [PY, 'tools/export_glare.py', '--location', L]),
        S(p + 'pickup-catalog', [PY, 'tools/import_pickup_catalog.py', '--location', L]),
        S(p + 'pickup-rewards', [PY, 'tools/link_pickup_rewards.py', '--location', L]),
        S(p + 'riding-start', [PY, 'tools/export_riding_start.py', st('glide'), f'local/assets/native/{L}/riding-start.json',
                               '--location', L], needs=['states']),
        S(p + 'race-event', [PY, 'tools/reference_race_event.py', st('glide'), '--output', f'local/assets/native/{L}/race-event.json'],
          needs=['states']),
        S(p + 'pickup-bindings', [PY, 'tools/probe_pickup_bindings.py', '--location', L], needs=['states']),
    ]
    if L != 'ARA1':
        steps.append(S(p + 'initial', [PY, 'web/prepare-ui.py', '--location', L], needs=['states']))
    if ev == 'backcountry':
        steps += [S(p + 'event-start', [PY, 'tools/export_backcountry.py', 'event-start', '--location', L], needs=['states', 'gamecube']),
                  S(p + 'npc', [PY, 'tools/export_backcountry.py', 'npc', '--location', L], needs=['states', 'gamecube'])]
    else:
        audit = 'local/browser-validation/countdown-rider-assemblies.json' if L == 'ARA1' else \
            f'local/browser-validation/{L}/countdown-rider-assemblies.json'
        steps += [S(p + 'rider-assemblies', [PY, 'tools/audit_rider_assemblies.py', '--discover', '--snapshot', st('countdown'),
                                             '--output', audit], needs=['states', 'gamecube']),
                  S(p + 'event-start', [PY, 'tools/export_event_start.py', '--location', L], needs=['states'])]
    steps += [
        S(p + 'event-instances', [PY, 'tools/export_event_instances.py', '--location', L], needs=['states']),
        S(p + 'scripted-instances', [PY, 'tools/export_scripted_instances.py', '--location', L], needs=['states']),
        S(p + 'light-tree', [PY, 'tools/export_light_tree.py', '--location', L], needs=['states']),
        S(p + 'rider-lighting', [PY, 'web/prepare-rider-lighting.py', '--location', L]),
        S(p + 'environment', [PY, 'web/prepare-environment.py', '--location', L], needs=['states']),
        S(p + 'web-2', [PY, 'web/prepare.py', '--location', L]),
    ]
    return steps


def post_steps(S, L):
    """The location's set pieces, sections and opponents, then its final draw batches (local/peak3-logs/post.sh order)."""
    ev = LOCATIONS[L]['event']
    p = f'post-{L}-'
    steps = [
        S(p + 'spline-setpieces', [PY, 'tools/export_spline_setpieces.py', '--location', L]),
        S(p + 'set-pieces', [PY, 'tools/export_set_pieces.py', '--location', L], needs=['states']),
        S(p + 'flags', [PY, 'tools/export_flags.py', '--location', L], needs=['states']),
        S(p + 'uv-scroll', [PY, 'tools/export_uv_scroll.py', '--location', L], needs=['states']),
        S(p + 'livecomp', [PY, 'tools/export_livecomp.py', '--location', L], needs=['states']),
        S(p + 'particles', [PY, 'tools/export_set_piece_particles.py', '--location', L, '--header'], needs=['states']),
        S(p + 'particle-textures', [PY, 'tools/export_set_piece_particle_textures.py', '--locations', L], needs=['iso']),
        S(p + 'stage-world', [PY, 'tools/export_stage_world.py', '--location', L], needs=['states']),
        S(p + 'attached', [PY, 'tools/export_attached_setpieces.py', '--location', L], needs=['states']),
        S(p + 'crowd', [PY, 'tools/export_crowd.py', '--locations', L], needs=['iso']),
        S(p + 'progress-meter', [PY, 'tools/export_progress_meter.py', '--location', L]),
        S(p + 'ready-state', [PY, 'tools/export_section_ready_state.py', '--location', L], needs=['states']),
        S(p + 'far-painter', [PY, 'tools/export_far_painter.py', '--location', L]),
        S(p + 'sections', [PY, 'tools/export_sections.py', '--location', L], needs=['states']),
    ]
    if ev in ('race', 'slopestyle'):
        steps.append(S(p + 'npc-riders', [PY, 'tools/export_npc_riders.py', '--location', L], needs=['states', 'gamecube']))
    elif ev == 'backcountry':
        steps.append(S(p + 'rivals', [PY, 'tools/export_backcountry.py', 'rivals', '--location', L], needs=['states', 'gamecube']))
    if ev in ('slopestyle', 'bigair', 'superpipe'):
        steps.append(S(p + 'freestyle-event', [PY, 'tools/export_freestyle_event.py', '--location', L], needs=['states']))
    steps.append(S(p + 'weather', [PY, 'tools/export_weather.py', '--location', L], needs=['states']))
    steps.append(S(p + 'web-3', [PY, 'web/prepare.py', '--location', L]))
    return steps
