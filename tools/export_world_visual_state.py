#!/usr/bin/env python3
"""World state that decides the visual-RNG (0x4FF018) draws of the post-rider world passes, read from any race
savestate (development export; git-ignored output), for seeding the browser core at a mid-run baseline
(web/stage_world.inc set_world_visual_state; web/compare-ps2-capture.mjs --seed-visual-rng loads CAPTURE.visual-state.json):

  crowd     CrowdMan2d *(gp-0x6F0): slots[128] (+0x8), countdowns[128] (+0x240 + 0x40*i), ring cursor (+0x2214)
  flags     flag manager wind {wind, base, delta, timer} (+ bits) and occupied slots (cloth, instances), as ready-state.json
  sections  0x101B60 activation object: game tick (+8 of the rider manager), last scan tick (+0xD0, -1 = rescan), last
            position (+0x20), parity (+0xD4), listed instances (+0xD8/+0xDC) with their parity bits (instance flags 0x200)
  snowfall_wind  the snowfall wind-layer timers (0x2E4F50 +0x90)
  riders    per roster slot the snow FX RiderBreath state (export_section_ready_state.rider_fx)
  visual_rng_4ff018  the six generator words; visual_lcg_4a3afc the shared visual LCG gp+0xA0C (board trail, sparks, snow)

usage: export_world_visual_state.py STATE.p2s --location BHP1 [--output OUT.json]
       (default output: STATE with .visual-state.json)
"""
import argparse, json, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import export_sections as X  # noqa: E402
import export_section_ready_state as R  # noqa: E402

GP = 0x4A30F0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('state'); ap.add_argument('--location', required=True); ap.add_argument('--output')
    a = ap.parse_args(); m = X.Memory(Path(a.state))
    cm = m.u(GP - 0x6F0)
    crowd = dict(slots=[m.u(cm + 8 + 4 * k) for k in range(128)], countdowns=[m.i(cm + 0x240 + 0x40 * k) for k in range(128)], cursor=m.u(cm + 0x2214)) if cm else None
    ready = R.export(m, a.location)
    act = m.activation(); listed = m.active_list()
    sections = dict(game_tick=m.tick(), last_scan=m.i(act + 0xD0), last=m.v(act + 0x20), last_bits=[X.fbits(x) for x in m.v(act + 0x20)], parity=m.u(act + 0xD4),
                    listed=[dict(resource=m.u(i + 0x78), parity=int(bool(m.u(i + 8) & 0x200))) for i in listed])
    out = dict(version=1, location=a.location, source=str(Path(a.state).resolve()), crowd=crowd, flags=ready['flags'], sections=sections,
               visual_rng_4ff018=ready['visual_rng_4ff018'], visual_lcg_4a3afc=m.u(0x4A3AFC), riders=ready['riders'], snowfall_wind=ready['snowfall_wind'])  # gp+0xA0C: trail / sparks / snow chunk LCG
    target = Path(a.output) if a.output else Path(a.state).with_suffix('.visual-state.json')
    target.write_text(json.dumps(out, separators=(',', ':')) + '\n')
    print(json.dumps(dict(output=str(target), crowd_slots=sum(1 for s in (crowd or {}).get('slots', []) if s != 0xFFFFFFFF),
                          flag_slots=len(out['flags']['slots']), listed=len(listed), game_tick=sections['game_tick'], last_scan=sections['last_scan'])))


if __name__ == '__main__':
    main()
