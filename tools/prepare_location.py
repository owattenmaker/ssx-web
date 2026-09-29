#!/usr/bin/env python3
"""Build every browser asset of one course/event location, in dependency order (docs/locations.md).

    .venv/bin/python tools/prepare_location.py BRA2            # disc + savestate exports, web package
    .venv/bin/python tools/prepare_location.py BHP1 --skip-import

Snow Jam (ARA1) keeps its historical per-script pipeline (web/package.json `npm run setup`); this
driver is for the other registered locations (tools/locations.py). Steps without their inputs are
skipped with a warning (for instance the savestate-derived evidence before the event savestates
exist), so a disc-only course is still loadable for free riding. After it, rebuild the core
(`sh web/build-core.sh`) because the event seeds are compiled in (web/generated/event_*_seed.hpp).
"""
import argparse, json, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from locations import location as location_info, state as location_state  # noqa: E402

PY = sys.executable


def run(*args, cwd=ROOT):
    print('+', ' '.join(str(a) for a in args), flush=True)
    subprocess.run([PY, *map(str, args)], cwd=cwd, check=True)


def build_initial(code):
    """web/prepare-ui.py build_initial(code): the course's own initial.json from its savestates."""
    import hashlib, struct, zipfile
    from locations import human_rider
    source = (ROOT / 'web/prepare-ui.py').read_text()
    body = source[source.index('def build_initial'):source.index(' return initial\n') + len(' return initial\n')]
    scope = dict(json=json, struct=struct, zipfile=zipfile, subprocess=subprocess, sys=sys, hashlib=hashlib, root=ROOT, Path=Path,
                 a=ROOT / 'local/assets/native', elf=(ROOT / 'local/disc/SLUS_207.72').read_bytes(),
                 location_state=location_state, human_rider=human_rider)
    exec(body, scope)
    initial = scope['build_initial'](code)
    (ROOT / f'web/public/assets/{code}/initial.json').write_text(json.dumps(initial))
    print(f'{code}: initial.json from', location_state(code, 'glide').name, flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('location'); p.add_argument('--skip-import', action='store_true', help='Reuse local/assets/native/<LOC> world/terrain/collision')
    code = p.parse_args().location; skip = p.parse_args().skip_import
    if code == 'ARA1':
        raise SystemExit('ARA1 uses `npm run setup` (web/package.json)')
    location_info(code)
    native = ROOT / 'local/assets/native' / code
    have = {kind: location_state(code, kind).exists() for kind in ('countdown', 'anchor', 'glide')}
    # 1. Disc-only world, sky, rails, painters, lighting (no savestate).
    if not skip:
        run('tools/import_world.py', '--location', code)
    run('tools/import_sky.py', '--location', code)
    run('tools/import_rails.py', '--locations', code)
    run('tools/export_environment_lighting.py', '--location', code)
    run('tools/export_irradiance.py')
    run('tools/export_local_lights.py', '--location', code)
    run('tools/import_stage_scripts.py', '--location', code)
    run('tools/disassemble_stage_scripts.py', '--location', code)
    # 2. Web world package (first pass: needed by the pickup catalog), painters, sun, glows.
    run('web/prepare.py', '--location', code)
    run('tools/export_fog_tree.py', '--location', code)
    run('tools/prepare_terrain_render.py', '--location', code)
    run('tools/export_sun_flare.py', '--location', code)
    run('tools/export_light_glow.py', '--location', code)
    if (ROOT / 'tools/export_glare.py').exists():
        run('tools/export_glare.py', '--location', code)   # world-painter type-6 glare pass (web/main.js glare.json)
    run('tools/import_pickup_catalog.py', '--location', code)
    run('tools/link_pickup_rewards.py', '--location', code)
    # 3. Event savestate evidence (tools/locations.py states; tools/ps2_navigate.py makes them).
    if have['glide']:
        run('tools/export_riding_start.py', location_state(code, 'glide'), native / 'riding-start.json', '--location', code)
        run('tools/reference_race_event.py', location_state(code, 'glide'), '--output', native / 'race-event.json')
    if have['anchor']:
        run('tools/probe_pickup_bindings.py', '--location', code)
    if have['glide'] and have['anchor']:
        build_initial(code)
    else:
        print(f'WARNING {code}: no event savestates; provisional disc-derived initial.json', flush=True)
        run('tools/export_course_initial.py', code)
    if have['countdown']:
        audit = ROOT / f'local/browser-validation/{code}/countdown-rider-assemblies.json'; audit.parent.mkdir(parents=True, exist_ok=True)
        if location_info(code)['event'] == 'backcountry':   # rolling start, no countdown (docs/backcountry.md)
            run('tools/export_backcountry.py', 'event-start', '--location', code)
            run('tools/export_backcountry.py', 'npc', '--location', code)
        else:
            run('tools/audit_rider_assemblies.py', '--discover', '--snapshot', location_state(code, 'countdown'), '--output', audit)
            run('tools/export_event_start.py', '--location', code)
        run('tools/export_event_instances.py', '--location', code)
        run('tools/export_scripted_instances.py', '--location', code)
        run('tools/generate_event_seed.py')
    run('tools/export_light_tree.py', '--location', code)
    run('web/prepare-rider-lighting.py', '--location', code)
    run('web/prepare-environment.py', '--location', code)
    # 4. Final web package with the event evidence (hidden helpers, pickups, rollers, rail runtime flags).
    run('web/prepare.py', '--location', code)
    print(f'{code}: done; rebuild the core (sh web/build-core.sh) if the event seeds changed', flush=True)


if __name__ == '__main__':
    main()
