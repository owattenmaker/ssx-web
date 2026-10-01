// Continuous original-game regression: replays private ARMSX2 captures (tools/ps2_capture.py,
// local/ps2-capture/runs) through the production pad path and fails if agreement regresses.
// Every gate runs to the END of its capture unless a case names the last exact tick (documented exceptions):
// rider physics (position/velocity bits), all 29 posed world bones incl. hair (BONE_SCAN_MAX=29), the score object and
// HUD slots whenever the capture records them, and for cam-* captures every DEFAULT_3 camera and compositor word.
// --sync-rng copies the recorded shared generator in each tick: the PS2's computer riders draw from
// it too, and the browser has none (a divergence of opponents, not of the human rider's code).
// Cases run in parallel (PAR=n, default: cores - 1); ONLY=name,... runs a subset.
import fs from 'node:fs';
import os from 'node:os';
import { execFile, execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
const runs = new URL('../local/ps2-capture/runs/', import.meta.url).pathname;
// Comparer reports go to a directory of this run's own: concurrent runs (the test loop, agents' targeted runs, scratch
// mirrors whose local/ links here) wrote the same runs/*.regression.json and could read each other's half-written files.
const reportRoot = fs.mkdtempSync(`${os.tmpdir()}/ssx-ps2-reports-`);
const reportPath = (name, ext) => { const f = `${reportRoot}/${name}.${ext}`; fs.mkdirSync(f.slice(0, f.lastIndexOf('/')), { recursive: true }); return f; };
process.on('exit', (code) => { if (!code) fs.rmSync(reportRoot, { recursive: true, force: true }); else console.log(`comparer reports kept in ${reportRoot}`); });
const END = Infinity;
const cases = [
  // name, extra args, last tick that must stay bit-exact, [through tick, max position error cm]
  // jump-tricks (8 KiB records, same baseline and script) is retired: jump-tricks16 below replays it with the animation state recorded.
  { name: 'air-tricks', args: ['--zoe'], exactThrough: END, why: 'D-pad spins/flips, flip landing, R2 grab+tweak, chords, crash and recovery' },
  // Air release and auto-complete (docs/HANDOFF.md "Air release"; scripts local/ps2-capture/scripts/air-release-*.json): tricks released
  // mid-rotation. Crow's Nest airs from the countdown anchor (--isolate) and Snow Jam glide jumps.
  { name: 'air-release/crows-invert', args: ['--zoe', '--event', '--finish-place', '4'], stageWorld: true, exactThrough: END, scoreThrough: END, boostThrough: END, why: "--finish-place 4 (9150 against the reference posting 61080 / 43420 / 19920 / 12120 / 5040: 0x239230 sets the meter 0 on the finish tick, the HUD meter follows). Crow's Nest: back flip released at -195 (inverted): phase-1 idle speed-up, phase 2 (308), phase 3; diagonal flip+spin released mid-rotation, a D-pad spin pressed during the auto-complete, in-flight stance switch 288; front flip with R2 grabbed while inverted, both released; 2079 the PS2 clears the boost meter at the finish and the browser does not (open, as peak3/perpendiculous 3996)" },
  { name: 'air-release/crows-adjust', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, why: "Crow's Nest: left-stick flip lean (-72) then a D-pad front-flip tap: the lean folds into the trick (phase 0, 405 from its target), the early release zeroes the targets and the rider turns back to neutral (682-694: phase 0 -> 3); stick spin lean then the opposite D-pad spin (same cancel); a back flip released inverted; R2 with the D-pad (the grab blocks the trick start)" },
  { name: 'air-release/rail-exit-spin', args: ['--zoe'], exactThrough: END, why: 'Snow Jam glide rail: Cross + D-pad left on the rail (control 2 on the rail: 245, then the prewind clip 255 selected from the retained currents before 1211F8 approaches them, 682), rail jump into a spin released and auto-completed (phase-2 stance switch 288), a front flip interrupted by a rail attach (control 5 exit 134CB0 bake), later rails' },
  { name: 'air-release/glide-land-rotating', args: ['--zoe'], exactThrough: END, why: 'Snow Jam glide: a late spin held through the touchdown at -126 (landing off rotation), a back flip released at -176 and auto-completed, a diagonal flip+spin released early (phase-2 stance switch 288)' },
  { name: 'air-release/pipe-wall', args: ['--zoe'], exactThrough: END, why: 'The Junction super pipe: charged wall jumps with flips released near the wall: a back flip released at -89 (phase 1 until the 499 crash), a front flip held into the wall, a diagonal flip held into the wall, a back flip released inverted at -192 sixteen ticks before the 1127 wall crash, a front flip with R2' },
  { name: 'air-release/pro-late-spin', args: ['--zoe', '--event', '--pro'], exactThrough: END, why: 'Pro map (INPUT2.MAP) Snow Jam race: Cross + stick late spins (0x1333E0 mode 2) released mid-rotation (mode 2 -> 1 at -88, phase 2 auto-complete at -180), Cross released with the tilt held (mode 1, crash 511), a flip+spin landing mid-rotation (744), a mode-1 prewind jump (760), a back flip held into the crash (901)' },
  { name: 'air-release/handplant-flip', args: ['--zoe'], exactThrough: END, why: 'Snow Jam handplant spot (handplant-spin timing) with a back flip started after the jump and Circle held: 0x133308 tests the handplant only in air phase 3 (and 0x107578 rejects an inverted rider), so the flip carries the rider past the spot (landing 723, rail 940)' },
  { name: 'air-release/pipe-depart-soft', args: ['--zoe'], exactThrough: END, why: 'The Junction: a hard left carve leaves the ground (13F194) in the same tick as a body contact: 108388 still sees motion 0 (11FE78(1) runs at 13F2CC after the contacts) and enters control 3 in the air (503; the port skipped it before core39), then flips and grabs with Circle, a rail (762), a reset (1344)' },
  { name: 'carve', args: ['--zoe'], exactThrough: END, why: 'carving/tuck/brake, 105398 platform contacts 408-410, crashbag hits 605/607 (static route, then RollerModifier entity), roller re-contacts 676-679' },
  // Same script with the RollerModifier pool watched (--watch 0x1CEF540:0x8A0): both crashbag rollers bit-exact every tick.
  { name: 'bag/carve-bag', args: ['--zoe'], exactThrough: END, rollerTicks: 344, why: 'crashbag RollerModifier creation 604/606, 35E850 dynamics and terrain contacts, rider re-kicks from 675' },
  { name: 'passive-inputs', args: ['--zoe'], exactThrough: END, why: 'L1 punch/charge clamp + attack-held control 4, passive departures (departure-tick tangents, air +370 = +180), 13AA48 air query with the core sphere mask 3 (731)' },
  { name: 'attack-mix', args: ['--zoe'], exactThrough: END, why: 'ground L1+R1 block/block cycle, R1 punch charge/release, re-press, L2/R2 chords (0x1163B0 + 131804 clamp)' },
  { name: 'attack-air', args: ['--zoe'], exactThrough: END, why: 'L1 punch held through a passive departure: attack-held control 4, release, 4->5 exit fade' },
  { name: 'neutral-3000', args: ['--zoe'], exactThrough: END, within: [1637, 0.0], why: 'long neutral run: passive takeoffs (114298 before the 13F358 clamp), landings, soft collision' },
  { name: 'event-start', args: ['--zoe', '--event'], exactThrough: END, within: [706, 0.0], why: 'grid countdown, push-off (grid route seed 0x490..0x4CC), tuck, carve, charged jump' },
  { name: 'event-race', args: ['--zoe', '--event'], exactThrough: END, why: '40 s race from the grid: carves, tucks, passive and charged jumps, spins, grabs, brake, two crashes and recoveries (899 landing-crash variant is drawn after the opponents, 1472 get-up reaction 314 before them), 106F78 hips/rail soft collisions 1989 and 2003' },
  { name: 'mix-glide', args: ['--zoe'], exactThrough: END, why: 'glide: boosting, hard carves, spin+grab, flip+grab, crash landing and crash exit to ground' },
  { name: 'handplant-ground', args: ['--zoe'], exactThrough: END, why: 'cruise handplant entry (failed attempts first), balance, phase-5 reflect launch, landing' },
  // handplant-spring (recorded before the capture tool logged the shared RNG: the 663 landing variant 62 leaf 84/85/86 draw read a zero RNG) is retired for handplant-spring-rng.
  { name: 'handplant-spring-rng', args: ['--zoe'], exactThrough: END, why: 'handplant released during INTO: phase-3 handspring, passive landing (variant 62 leaf drawn from the recorded shared RNG)' },
  { name: 'handplant-leanR', args: ['--zoe'], exactThrough: END, why: 'HandplantBalance input, phase-4 through exit, 796 soft collision' },
  { name: 'handplant-rail', args: ['--zoe'], exactThrough: END, why: 'phase-6 exit to rail (control 11 -> 106848, style 4 sideways stance alignment), grind' },
  { name: 'rail-air-fence', args: ['--zoe'], exactThrough: END, why: 'air (control 5) rail attach: 13AD20 inside 106848, 134CB0, 13AF28 grind, rail lost, passive air; the 785 fence (688904) is a type-16 node entity every collector skips' },
  // Left stick on a rail (RailBalance = LStickR-LStickL -> 0x113F38 unnegated): left slides the rider toward -right (screen left).
  { name: 'rail-balance-lr', args: ['--zoe'], exactThrough: END, why: 'ground rail attach (style 1 50-50), left stick left 0.6 then right 0.6 while grinding (+0x22C steer, +0x238 balance, 13AF28 slide), exit and landing' },
  { name: 'rail-balance-slide', args: ['--zoe'], exactThrough: END, why: 'handplant exit to a style-4 boardslide, left stick left then right (balance clips/lean follow +0x238), exit' },
  { name: 'handplant-nat36', args: ['--zoe'], exactThrough: END, why: 'failed cruise then natural-air (control 4) handplant entry' },
  { name: 'handplant-spin', args: ['--zoe'], exactThrough: END, why: 'charged jump, spin (control 5) handplant entry after the 0x134CB0 bake' },
  // Board press (control 1, engine/board_press.hpp): boardPress also checks +0x330, the control-1 object
  // (owner+0x1D0) and the channel-2 semantic on every tick (--board-press, BP_ALL_SEMANTICS).
  { name: 'boardpress-nose', args: ['--zoe'], boardPress: true, exactThrough: END, why: 'nose press: 1161D0 entry (24), phase 0/1 (28), release, phase 3 (25), back to cruise' },
  { name: 'boardpress-tail', args: ['--zoe'], boardPress: true, exactThrough: END, why: 'tail press (32/36/33) and release' },
  { name: 'boardpress-ollie', args: ['--zoe'], boardPress: true, exactThrough: END, why: 'R3 ollie (1307B8: 114298 charge 1, motion 1, control 5), air +0x330, landing re-entry into control 1 (26)' },
  { name: 'boardpress-pivot', args: ['--zoe'], boardPress: true, exactThrough: END, why: 'BoardPivot kick/rotation (29/30), stance flip 12FEC8, spring back, 131428 finalise' },
  { name: 'boardpress-circle', args: ['--zoe'], boardPress: true, exactThrough: END, why: 'full right-stick circle: pivot rotation through +-pi, 37/38 and 29/30, 12FFF8 in the air' },
  { name: 'boardpress-repress', args: ['--zoe'], boardPress: true, exactThrough: END, why: 'release then re-press in phase 3 (back to phase 0), nose then tail' },
  { name: 'boardpress-long', args: ['--zoe'], boardPress: true, exactThrough: END, why: 'full depth held >1 s: 130DD0 hard crash 10EB30(358) in the controller slot, recovery' },
  { name: 'boardpress-jump', args: ['--zoe'], boardPress: true, exactThrough: END, why: 'Cross during a tail press: 1162C8/131348 -> control 2 (245), charged jump' },
  { name: 'boardpress-air', args: ['--zoe'], boardPress: true, exactThrough: END, why: 'tail press in the air (+0x330 = 2, air adjust 0x28C/0x298), landing into control 1 (34)' },
  { name: 'boardpress-pivot-ollie', args: ['--zoe'], boardPress: true, exactThrough: END, why: 'tail pivot then R3 ollie from a pivot (35), landing' },
  { name: 'boardpress-rail', args: ['--zoe'], boardPress: true, exactThrough: END, why: 'press on a rail: control 7 -> 1161D0 (131E80), control 1 with motion 4, rail lost, 12FFF8; 821 106F78 hips/rail hard crash, 13F358 clamp after the contacts' },
  { name: 'boardpress-railair', args: ['--zoe'], boardPress: true, exactThrough: END, why: 'tail press in the air onto a rail: 106D9C (34, control 1), phase 1 on the rail, 12FFF8; 792 air instance contact' },
  { name: 'boardpress-railjump', args: ['--zoe'], boardPress: true, exactThrough: END, why: 'rail press then Cross: control 2 on the rail (245), jump off; 825 ground rail re-attach (108A48 with the 1211F8-approached +0x25C tolerance)' },
  // Left-stick air adjust (AirAdjRotLR/FB, control 5 0x133308): bonesThrough also requires all 24 body/board world
  // bones (cached +0x2C transforms, BONE_SCAN) to be bit-identical through that tick.
  { name: 'air-steer-lr', args: ['--zoe'], exactThrough: END, why: 'charged jumps holding the left stick left/right (air adjust past ~90 deg: 0x135BE0/0x114DB8 in-flight stance switch, 288), kind-11 adjust clips 297..304 fading out after landing' },
  { name: 'air-steer-fb', args: ['--zoe'], exactThrough: END, why: 'left stick up/down in the air (flip adjust); steeply pitched landing body contact' },
  { name: 'air-steer-diag', args: ['--zoe'], exactThrough: END, why: 'diagonal, partial-magnitude and circling left stick in the air, passive departures' },
  { name: 'air-steer-passive', args: ['--zoe'], exactThrough: END, why: 'left stick in passive (control 4 -> 5) flights incl. a 155-tick airtime; landing revives the fading landing clip (a2 = 0); the 1145 crash landing is exact since 13AA48 queries the touched-down body (106538 push)' },
  { name: 'air-steer-tuck', args: ['--zoe'], exactThrough: END, why: 'tuck held through passive departures (control 4 CruiseTurn/CruiseCrouch, cycle 10) with left stick turns' },
  // Controller Settings "Pro" (INPUT2.MAP, derived countdown local/reference/pcsx2/characters/fe-screens/options/snow-jam-countdown-pro.p2s):
  { name: 'pro-event', args: ['--zoe', '--event', '--pro'], exactThrough: END, why: 'Pro map: prewind spin on the direction with Cross, late spin (Cross + D-pad / stick in the air, 0x1333E0 modes 2 -> 1), air adjust = Turn/Tilt, Triangle hand plant bit, Circle, a crash and a rail' },
  { name: 'air-steer-event', args: ['--zoe', '--event'], exactThrough: END, why: 'from the grid: tuck, charged jump holding left (in-flight switch at 399), second jump with up+right air adjust, crash landing' },
  // Trick scoring (docs/tricks-scoring.md): these captures also record the score object *(rider+0x790) and its 44 HUD
  // message slots; scoreThrough requires every score word and slot header to be bit-identical through that tick.
  { name: 'score-air-tricks', args: ['--zoe'], exactThrough: END, why: 'D-pad spins/flips (119C98 trick start, 117FE0 spin dial and landing messages), R2 grab+tweak, chords, takeoff/landing boundaries, combo clock, hard crash 119B08 and recovery' },
  { name: 'score-combo', args: ['--zoe'], exactThrough: END, why: 'quick re-jumps: combo count/points 117638, 1.5 s combo clock, grab+tweak on a hop, bail with pending points (-0.25 via 10E098)' },
  { name: 'score-repeat', args: ['--zoe'], exactThrough: END, why: 'the same spin three times: trick history 1190F0 and the repeat-divided points / 0x24 popup' },
  { name: 'score-grabs', args: ['--zoe'], exactThrough: END, why: 'several grab chords in one air (grab list +0x60), hold time, spin+grab, long grab to landing' },
  { name: 'score-rail', args: ['--zoe'], exactThrough: END, why: 'rail grind distance/threshold scoring (117C28 +0x24, 119210 table 459F68) and rail exit boundaries' },
  { name: 'score-uber', args: ['--zoe'], exactThrough: END, why: 'Tricky (poked meter 1, tier 1): R2+Square air Uber (id 42), Uber landing crash with pending points, reset 119368/119E38, passive flights; 739 hair: 120378 selects from the pose 11D660 -> 11EB98 builds at the placement' },
  { name: 'score-uber6', args: ['--zoe'], exactThrough: END, why: 'tier 6: the air Uber also counts as a super Uber (+0x58)' },
  { name: 'score-rail-uber', args: ['--zoe'], exactThrough: END, why: 'rail Uber (control 12, 119938/119958/117E58), landing commit with Uber tier progression 10E9B4 (tier 2, meter refilled)' },
  // Metro-City (BRA2, docs/locations.md): the comparer loads the course from the capture manifest location.
  { name: 'metro-event-start', args: ['--zoe', '--event'], exactThrough: END, why: 'Metro-City grid countdown, push-off, tuck, carve, charged jump (BRA2 event seeds)' },
  { name: 'metro-event-race', args: ['--zoe', '--event'], exactThrough: END, why: 'Metro-City race from the grid, end to end: 1094 tuck + boost press selects semantic 8 (131878 reads +0x2FC after 114130), 2049 soft control 3 attaches to a rail (12E778 -> 106848)' },
  // Same script with the human Lighting property object watched (0x58BF00 + its 2C0778 wrapper): the area-selected bank follows the PS2.
  { name: 'metro-event-race-light', args: ['--zoe', '--event', '--lighting'], exactThrough: END, lightingReferences: 2300, lightingScalarsThrough: 1550, why: 'Metro-City Lighting painter (type 11) by area: BPBRR/BPBRB/BPBRY bright banks on all 2300 ticks; 1551 the PS2 re-seeds the rim scalar (trigger unknown)' },
  { name: 'metro-glide-neutral', args: ['--zoe'], exactThrough: END, why: 'Metro-City glide (tick 620 checkpoint, BRA2 glide seed): 1500 neutral ticks, drops, landings, soft collisions' },
  { name: 'metro-glide-carve', args: ['--zoe'], exactThrough: END, why: 'Metro-City carve/tuck/brake/jumps, end to end: 1048 air contact restarts the predictor (105D98 -> 1135B8), 1208 soft collision re-plays 59 over its fading copy (inherited, 108388 plays without force)' },
  { name: 'metro-jump-tricks', args: ['--zoe'], exactThrough: END, why: 'Metro-City charged jumps, spins, grabs, rail grind, end to end: 1491 air-entry rail clip 68 completes through 104C38 (plays 18, keeps 68 fading; completion table 0x456990 kinds 6/7)' },
  { name: 'metro-air-tricks', args: ['--zoe'], exactThrough: END, why: 'Metro-City D-pad spins/flips and rail grind, end to end: 888 completion 104C38, 936 jump on a rail runs control 7 exit 132048, 981 air release resets the default root and clears +0x328 (12E9B8)' },
  { name: 'metro-mix-glide', args: ['--zoe'], exactThrough: END, why: 'Metro-City boosting, hard carves, spin/flip+grab, crashes, end to end: 719 ragdoll impact restarts the crash predictor (105D98 -> 1135B8 at rider+0x2E4), 1096 crash landing probe shares the rider+0x864/+0x868 contact caches' },
  // The Junction super pipe (BHP1, one rider, event variant 2; docs/locations.md "Super pipe physics"). Pipe behaviour comes from
  // the contacted patch: rider+0x2D4 = patch flags (0x20 keeps a vertical-wall takeoff at full speed, 114A5C) and rider+0x434 =
  // the patch's location id (1218D0 -> 22E0E0; 11..13 are the half pipes: 13C948 auto boost, 114298 ground-focus scale).
  { name: 'pipe-event-start', args: ['--zoe', '--event'], exactThrough: END, why: 'The Junction countdown, push-off (rider type 0x31 -> 11 at the first contact), pipe run' },
  { name: 'pipe-neutral', args: ['--zoe'], exactThrough: END, why: 'riding the pipe from the glide: vert lip takeoffs (patch flag 0x20), straight-up air back into the pipe, wall landings' },
  { name: 'pipe-air', args: ['--zoe'], exactThrough: END, why: 'wall carves and charged jumps off both walls with spins/grabs; 13AA48 soft contact restarts the predictor (1135B8); crash exit with Cross held (131608 latch)' },
  { name: 'pipe-air-grabs', args: ['--zoe'], exactThrough: END, why: 'spins/flips/grabs timed to the pipe airs' },
  { name: 'pipe-handplant', args: ['--zoe'], exactThrough: END, why: 'Circle at the lip: handplants on the coping (rail flags 0x20002, control 11), leaning plants, exits back into the pipe' },
  { name: 'pipe-uber', args: ['--zoe'], exactThrough: END, why: 'full meter (poke): R2/L1+R1/L2 + Square Ubers in the pipe, Uber crash and reset (116120 reason 1: 11A088 "Wrong Way!" slot 0x33), crash landing after the reset drop (13AA48 at the touched-down body), soft crashbag contact on the departure tick (108388 gates on motion 0; 13F2CC -> 1399E0 seeds the flight after 105398, was 1143)' },
  { name: 'bag-bhp1/uber-bag', args: ['--zoe'], exactThrough: END, rollerTicks: 15, why: 'pipe-uber with the BHP1 roller pool watched: crashbag 108559 RollerModifier built on the departure tick 1142, the flight seeded after the bounce clears it (no 1143 re-contact), 15 roller ticks' },
  { name: 'pipe-tricks', args: ['--zoe'], exactThrough: END, why: 'flips, spins+grabs, tweaks, chords off both walls; inverted flip landing at 1363: the landing probe 13A7B0 only sees the rider query scope 332DB8 built from the query bounds every third tick (was 1361, docs/locations.md)' },
  // Full event runs from the countdown anchor (--event; truncated before the PS2 results/replay restarted the tick counter).
  { name: 'celebrate/pipe-2nd', args: ['--zoe', '--event', '--finish-place', '1'], stageWorld: true, exactThrough: END, scoreThrough: END, boostThrough: END, why: 'The Junction with the posted slots poked (0x536640 and the handler rounds: 162880 / 1000 / 0 / 0 / 0) so the 2000-point run finishes 2nd: 0x239230 keeps rider+0x100 = 1 (place < 3; the arm, 315) and sets the boost meter to 0.35 on the finish tick (4403)' },
  { name: 'pipe-finish', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, why: 'score exact to the end since the 12A250 commit gate (a landing after every human finished commits nothing); neutral 2:00 super pipe run from the grid to the finish (4403 finish marker in the air, control 10 only after landing: 116378 reads the controller slot); 2862 the pointa pickup pays 2000 once its MagnetModifier reached the rider (stage world: builtin90, web/stage_world.inc); 4498 score+0xA4 after the finish landing' },
  // Peak 1 freestyle (docs/slopestyle-bigair.md): Crow's Nest big air and R&B slope style from their countdown anchors, with the
  // course's freestyle configuration (web/public/assets/<X>/freestyle-event.json: mode, handler, kind, 0x4D33B8, time limit) and
  // the stage world (point icons 10E8B8/119608, multiplier icons 10E830/119448, reset planes, MeshAnim break pieces).
  { name: 'peak1/crows-event-tuck', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, why: "Crow's Nest (ABA1) tuck/carve run from the grid over the kickers to the finish: 2000-point icon 1103, reset plane 2002 (control 9), finish in the air 2057 (1193E0 closes the combo); the 2073 landing commits nothing because every human has finished (12A250 -> 11A228 returns)" },
  { name: 'peak1/rnb-event-tuck', args: ['--zoe', '--event'], stageWorld: true, exactThrough: 6671, scoreThrough: 6675, bonesThrough: 1560, why: "R&B (ASS1) slope style tuck/weave run from the grid (Moby rides; --isolate capture): x5 multiplier icon 845 (10E830/119448), trainbox crash 981, checkpoint 1 at 3302 (+60 s and the 0x29 '+60' popup, 1194C0/0x2398E8); 6672 a control-3 soft collision lands 0.03 cm off (open); bones 24/25 from 1561 (open)" },
  // Peak 2 (docs/peak2.md): tuck/weave runs from each countdown anchor (--isolate, section/chunk/viewer watches; the first run
  // of local/ps2-capture/runs/peak2/<loc>-full, cut before the event restart).
  { name: 'peak2/stylemile-event-tuck', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, why: 'Style Mile (DSS2) slope style from the grid (the opponent rides; isolated): 9000 ticks of tuck/weave with the +90 / +60 checkpoints; physics, bones, score and boost exact to the end' },
  { name: 'peak2/dss2-full', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, why: 'Style Mile (DSS2) from the grid, the visual-parity sweep run (tuck with +-0.5 weaves; rails, crashes 8, resets 9, soft collisions 3): physics, bones, score and boost exact for all 9000 ticks' },
  { name: 'peak2/launch-event-tuck', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, why: 'Launch Time (CBA2) big air from the grid, tucked over the kickers to the finish (Final run, 0 pts at 2844); physics, bones, score and boost exact to the end' },
  { name: 'peak2/cra3-race-ai', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, why: 'Ruthless Ridge (CRA3) race from the grid, 5999 ticks of tuck/weave with the five computer riders racing (--ai-state --isolate capture; its rng-order.json places the human draws among theirs: the 1383 strong soft impact 108388 spin draw lands between the computer riders\' draws): physics, bones, score and boost exact to the end' },
  { name: 'peak2/dra4-race-ai', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, why: 'Intimidator (DRA4) race from the grid with the five computer riders (--ai-state --isolate, rng-order.json): exact through the first passive departure; 835, the first air tick after it (PS2 control 4, turn 0.047), lands 1-2 ulp off in all three velocity components (horizontal drag factor 0.99666679 on the PS2 vs 1 - 1/300 in the browser, gravity step 31.66687 vs 31.66663; open)' },
  { name: 'peak2/schizo-event-tuck', args: ['--zoe', '--event', '--finish-place', '5'], stageWorld: true, exactThrough: END, scoreThrough: END, bonesThrough: 3105, boostThrough: END, why: 'Schizophrenia (CHP2) super pipe from the grid to the pipe end (3899): physics exact to the end; 3106 the rider hops off a rail for two ticks onto the next one (control 7 -> 4 -> 7): the takeoff passes 114298 s1 (the ramp branch) to 119E38, as on the PS2 (web/rail_gameplay.inc); on that tick only the posed bones sit 1.6 / 3.7 cm off in x / y (the PS2\'s world bones keep the pre-takeoff horizontal translation; exact again from 3107; open); 3492 the finish (6th: 3768 against the reference posting from 223960): 0x239230 sets the boost meter by place (0; --finish-place 5) before the HUD update, so HUD slots 5/6 follow at once' },
  // Peak 3 (docs/peak3.md): tuck runs from each countdown anchor / the Throne's rolling start (--isolate, section/chunk/viewer
  // watches; local/ps2-capture/runs/peak3/<name>-full, the gates cut before any event restart by local/peak3-logs/first_run.py).
  { name: 'peak3/much-2-much-event-tuck', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, why: 'Much-2-Much (EBA3) big air from the grid, tucked over the kickers to the finish (3167); physics, bones, score and boost exact to the end' },
  { name: 'avalanche/eba3-rock-hit', keepCheck: true, args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, why: 'Much 2 Much after the rock slide (trigger 540): steered past the tumbling rockslide_1001 at 1.8 m around 1111; its collision (the builtin-0 Object with the AvaSpline, entity route, bounds from the 2D9C00 matrix of the previous tumbler step) must not touch the rider (web/avalanche_gameplay.inc)' },
  { name: 'avalanche/eba3-rock-hit-a', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, why: 'Much 2 Much: the rider hits the tumbling rockslide_1001 at 1113 (105398 instance contact on the moving entity, rigid response 0x360B60 = 1); with the rocks static (core50) it left at 1113' },
  { name: 'avalanche/eba3-rock-hit-b', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, why: 'Much 2 Much: the same rock hit one tick earlier (1112) from a later, shorter steer' },
  { name: 'peak3/perpendiculous-event-tuck', args: ['--zoe', '--event', '--finish-place', '5'], stageWorld: true, exactThrough: END, scoreThrough: END, bonesThrough: 3866, boostThrough: END, why: 'Perpendiculous (EHP3) super pipe from the grid to the pipe end (4403): physics exact to the end; 3867 the posed root leaves the PS2 by ~3 cm (open); the finish (3996, 6th: 3660 against the reference posting from 509000) clears the boost meter (0x239230 by place, --finish-place 5)' },
  { name: 'peak3/kick-doubt-event-tuck', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, scoreThrough: 3780, bonesThrough: 3781, boostThrough: 3780, why: 'Kick Doubt (ESS3) slope style from the grid (the opponent rides; isolated): 3600 ticks of tuck over the course incl. an airborne soft collision landing into control 0 (2234); 3781 the 1:00 time-out, exact since WS3 selects Race on the countdown end (core race_bridge.cpp gate_go: 125228 reads race ticks = total - 179); before it the PS2 entered control 10 one tick before the browser' },
  { name: 'peak3/gravitude-event-tuck', args: ['--zoe', '--event'], stageWorld: true, exactThrough: 6883, scoreThrough: 6883, bonesThrough: 6883, why: 'Gravitude (ERA5) race from the grid, tucked (computer riders isolated): airborne soft collision landing 623; 2964 the fall-reset plane (instance 175917, box node) is found only once the rider+0x860 query scope (332DB8, every third tick) holds it, as 104E70 -> 334458 walks that list; 6884 the browser leaves the ground one tick early (open)' },
  { name: 'peak3/the-throne-tuck', args: ['--zoe', '--event'], stageWorld: true, exactThrough: 6104, scoreThrough: 6104, bonesThrough: 696, why: 'The Throne (EBC3) Rival Time rolling start, tucked: physics and score exact through 6104. 4057: two tree rails that share an end point tie in 0x334680; the octree walk order (later segments first in a cell) picks treerailhevbb_1056 as the PS2 does. 4903: 1057B8\'s air landing on a scenery top from control 5 (134CB0 bake, control 13 -> 5, clip 268). 6105 the browser lands one tick early (open); bones exact through 699 since the Continue\'s 11D390 placement runs at the rolling start (web/start_gameplay.inc, docs/weather.md 12)' },
  { name: 'peak3/gravitude-race-ai', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, why: 'Gravitude (ERA5) race from the grid with the five computer riders racing (--ai-state --isolate, weave script; rng-order.json places the human draws): 621 a soft collision on a passive departure tick, 670 an airborne soft collision ending in the air (control 3 -> 4, 12F730 requests control 5 on the next tick); 1505 a passive landing with a soft collision on the landing tick (13AA48 after the touchdown keeps the +0x180 filter and its air response; 108388 sees motion 0 and enters control 3); physics, bones and score exact to the end' },
  // Streamed Peak 1 world (docs/peak-mountain.md "Captures"): free ride from Green Base Station into the A_ARA1 connector, the
  // Unload trigger (3419: A and five neighbours 2 -> 7, eviction 3427) and the ARA1 read. Physics bit-exact through the
  // read completion and to the end: the 3666 step was the route heading +0x4CC from the wrong region row (112180 attaches to runtime kind 1 = exported kind 0, index 0) and the bank delivery during the read (web/peak-capture.mjs). Bones and
  // the trick score are not seeded for this baseline yet (bonesThrough/scoreThrough 0: reported, not gated).
  { name: 'peak1-fr-aara1-glide', args: ['--zoe', '--course', 'PEAK1', '--seed-limit', '--seed-idle'], exactThrough: END, bonesThrough: 0, scoreThrough: 0, why: 'Peak 1 free ride across the A -> A_ARA1 unload/eviction boundary (streamed rows from the capture, section activation kills the challenge reset planes)' },
  // Streamed Peak 3 world (docs/peak3.md "PEAK3 world"): The Throne free ride (runs/peak3/fr-throne-late from record 14302, the
  // cleaned tick-14302 state = local/ps2-capture/peak3/derived/fr-ebc3-14302.p2s as the PEAK3 seed) through the EBC3 Unload
  // (14422: EBC3 2 -> 7, E and its four connectors 3 -> 6 -> 1). Physics bit-exact since the instance walks follow the octree
  // order (2026-09-26; before, 14469's control-3 soft collision left the PS2). 15035 (round 2, 2026-09-26): a lost rail's passive
  // exit (132770 -> control 4) lands on a scenery top the same tick: 1057B8's surface landing (10E910, 119E38, 11FEC8 13 then 5,
  // clip 268; PS2 entry probe of 11FEC8: ra 0x105CC0 / 0x105CCC), so the 15036 re-attach is control 5's (134CB0 about the pose
  // pivot, 1 ulp). 15247: a soft collision (control 3) the browser does not enter (open). Bones / score not seeded (0).
  // The whole mountain (docs/peak3.md section 6; tools/ps2_autopilot.py closed-loop runs from the objectives card's Continue,
  // world MOUNTAIN seeded from the card state, its stage world loaded as the browser's: web/peak-capture.mjs): the All Peak
  // Race's first 14000 ticks (The Throne -> EBC3_E -> Black Top Station). Past the crash contacts since 2026-09-26: 785 (104E70's
  // normal sum follows the octree order of the contacted instances), 3581 (13AA48 after a touchdown filters with +0x180), 11879
  // (the score ticks while a crossing has no path bank). 11947 (round 2, 2026-09-26): 13C7A8's landing speed scale reads the
  // game tick 1298C8 (rider manager +8), which a time challenge's crossing restarts: world state 10's background rider reload
  // (235080 sub-state 3 -> 128A10 -> 1297C8, after the destination's NIS script read) sets it to 0 at 11883. The comparer drives
  // the restart from the record's tick field (web/peak-capture.mjs -> core game_tick_restart), as it does the rows; the page's
  // model is web/peak_world.inc. Physics exact to the end; 12472 the E station split HUD 0x2A, posted by the page's
  // web/free-ride.js (not by the comparer).
  { name: 'allpeak/apr-start', keepCheck: true, args: ['--zoe', '--course', 'MOUNTAIN', '--peak-run'], exactThrough: END, bonesThrough: 0, scoreThrough: 12471, why: 'All Peak Race from Continue on the whole-mountain world: physics to the end across the EBC3_E crossing\'s game-tick restart (11883, driven from the record\'s 1298C8); 12472 the E station split (HUD 0x2A), which the page posts (web/free-ride.js) and the comparer does not' },
  // The Peak 2 Race (seed MOUNTAIN2: its objectives card at Ruthless) on the same world: the first 6000 ticks (DBC2 at Ruthless;
  // the Unload into Yellow station D is at 13817). Physics exact to the end (the 1258 ground crash among the rocks: 104E70's
  // octree order). Score to the end (round 2, 2026-09-26): the PS2 collects mdl_DBC2_collecta_1001 at 1841 during a crash after
  // its MagnetModifier pulled it to the rider. The seed MOUNTAIN2 names the world MOUNTAIN, whose sections / stage world
  // ("location": "MOUNTAIN") did not run under that name, so no slot-1 program built the magnet (web/streamed_world.hpp
  // browser_same_world; the page always runs MOUNTAIN).
  { name: 'allpeak/p2r-start', args: ['--zoe', '--course', 'MOUNTAIN2', '--peak-run'], exactThrough: END, bonesThrough: 0, scoreThrough: END, why: 'Peak 2 Race from Continue on the whole-mountain world' },
  // The All Peak Jam (seed MOUNTAINJ: its objectives card, mode 11): the pilot's charged jumps and R1 grabs from Continue, the
  // first 6000 ticks at The Throne. Physics exact to the end: the fallingpatha LiveComp (program 19 on its trigger's contact,
  // 2326) moves the path's collision (0x334888 on the entity's nodes), so the 2661 get-up no longer meets it at rest. Score to the
  // end: the 5943 collectible (2000) is taken through its magnet as in p2r-start (round 2).
  { name: 'allpeak/apj-start', args: ['--zoe', '--course', 'MOUNTAINJ', '--peak-run'], exactThrough: END, bonesThrough: 0, scoreThrough: END, why: 'All Peak Jam from Continue on the whole-mountain world (jumps, grabs, the trick score)' },
  // Conquer the Mountain free ride on the whole mountain (docs/ctm-parity.md "The whole mountain"; seed MOUNTAINF = the capture's
  // baseline ctm-parity/mountain/states/frdra4-lodgeD.p2s, Zoe on Intimidator at tick 3370; tools/ps2_autopilot.py route DRA4 -> DRA4_A ->
  // Green Base Station, local/ps2-capture/runs/ctm -> ctm-parity/mountain/runs). Physics, bones and the trick cash exact through 8403:
  // the 7218 combo payout shows in HUD slot 0x19 on its tick (117FE0 reads C+0xAC4 after the awards; engine/score_object.cpp careerCash, core47). 8404: the PS2 finds the ground
  // on mdl_DRA4_highwayRebuild_2049 (patch -1, normal (0.96, 0.28, 0)) a tick before the browser (open; not the 332DB8 scope: all
  // instances admitted from 8380 still meet it at 8405). Skipped on a core without the MOUNTAINF seed.
  { name: 'ctm/fr-dra4a-full', coreHas: 'MOUNTAINF', args: ['--zoe', '--course', 'MOUNTAINF'], exactThrough: 8403, bonesThrough: 8403, scoreThrough: 8403, why: 'CTM free ride on the whole mountain from Intimidator (seed MOUNTAINF): 7218 the combo payout in HUD slot 0x19 on its tick; 8404 the PS2 lands on a highway piece a tick earlier (open)' },
  // Conquer the Mountain events in the streamed world (docs/ctm-events-in-world.md sections 4 and 6): each build stage's gate, ready before
  // the stage exists. `pending: SWITCH` cases are skipped unless PENDING=1 (or ONLY names them); then they are scored and reported, never
  // failed, with `today` the current result on the event package / today's streamed path. When the stage lands, drop `pending`, move the
  // case to its new comparer args (the in-world path) and keep exactThrough as the gate. `unscored: WHY` cases cannot be scored yet.
  // The captures (silent ARMSX2, derived states) are local/ctm-events/caps/, linked from local/ps2-capture/runs/ctm-events/.
  // --ctm-countdown seeds what core event_grid_start keeps from the carried rider (the motion-0 stamps: 13C7A8's push-off speed
  // scale; the boost words; the normals). The human alone is exact until its idle upper reaction (115D48 at 1 s idle, 302 / 301),
  // which looks at the computer riders within 10 m; the six-rider cases below are exact on the human to the end.
  { name: 'ctm-events/c0a-race', args: ['--zoe', '--event', '--ctm-countdown'], exactThrough: 309, bonesThrough: 300, scoreThrough: 309,
    why: 'CTM Snow Jam qualifier (first heat, ridden in from free ride) from its countdown: 302 the idle reaction picks a computer rider (six-rider case)' },
  { name: 'ctm-events/c0c-race', args: ['--zoe', '--event', '--ctm-countdown'], exactThrough: 318, bonesThrough: 300, scoreThrough: 318,
    why: 'CTM Snow Jam semi-final (WS13 Next heat: 230180, the gondola; the qualifier boost meter 0.621 carried) from its countdown: 301 the idle reaction picks a computer rider (six-rider case)' },
  // The first heat in one capture (local/ctm-events/capture_card.py): the Snow Jam arrival, free ride into the gate (2822), WS1's hold
  // (2853), the card, the countdown and 1150 race ticks, as the page runs it (compare-ps2-capture.mjs --ctm-in-world: the gate's
  // seeds and document, the hold, event_grid_start at the Continue, which keeps the stage world: the Big Challenge markers stay the
  // Hide nodes 308DB8 made at 2066, flg_ARA1_BigCFlag_1002 at race tick 829). Exact through race tick 1507. 1508: a soft collision on
  // the PS2 with no instance contact (the 105D98 notify count unchanged): a computer rider, which stage 4 brings. 2822: the gate's
  // score commit 12B180 and the event HUD bank are not seeded.
  { name: 'ctm-events/c0a-full', args: ['--zoe', '--course', 'PEAK1', '--peak-arrival', '--ctm-in-world', 'ARA1'], exactThrough: 1507, bonesThrough: 0, scoreThrough: 2821,
    why: 'CTM Snow Jam first heat in the streamed world, arrival -> free ride -> gate -> hold -> card -> race: 1508 a computer rider contact (stage 4)' },
  { name: 'ctm-events/c0b-ass1-arr', pending: 'eventInWorld', bonesThrough: 0, scoreThrough: 0, today: 2478, args: ['--zoe', '--course', 'PEAK1', '--peak-arrival'], exactThrough: END,
    why: 'R&B Transport arrival -> free ride into the RaceRideState gate (2448) -> WS1 (slope style): exact until the NIS hold at the gate + 31 (2479), which the port lacks' },
  { name: 'ctm-events/c0b-bra2-arr', pending: 'eventInWorld', bonesThrough: 0, scoreThrough: 0, today: 2413, args: ['--zoe', '--course', 'PEAK1', '--peak-arrival'], exactThrough: END,
    why: 'Metro-City Transport arrival -> the gate (2383) -> WS1: exact until the NIS hold at 2414, which the port lacks (the placement tick\'s 0.09 cm/s was the unseeded route heading +0x4CC)' },
  { name: 'ctm-events/c0a-gate-arr', pending: 'eventInWorld', bonesThrough: 0, scoreThrough: 0, today: 2852, args: ['--zoe', '--course', 'PEAK1', '--peak-arrival'], exactThrough: END,
    why: 'Snow Jam Transport arrival (menus/ctm/state-transport-confirm, its stale menu hook removed) -> 314 neutral + 20 left -> the RaceRideState gate (2822) -> WS1: exact until the NIS hold at the gate + 31 (2853), which the port lacks' },
  { name: 'peak3/fr-throne-unload', keepCheck: true, args: ['--zoe', '--course', 'PEAK3', '--seed-limit', '--seed-idle'], exactThrough: 15246, bonesThrough: 0, scoreThrough: 0, why: 'Peak 3 free ride across the EBC3 Unload (seed fr-ebc3-14302): 15035 a lost rail\'s passive exit lands on a scenery top (1057B8: control 13 -> 5, clip 268), 15036 the control-5 re-attach bakes about the pose pivot (134CB0); 15247 a soft collision (control 3) the browser does not enter (open)' },
  // Course limits (docs/peak-mountain.md "Course limits in free ride"): hard right into The Throne's ice blocks from the Peak 3 seed; back-to-back
  // soft collisions 3 ticks apart: 312AE8 reads the FIRST channel-2 sequence, not the previous soft's completed clip still fading (14776).
  { name: 'course-limits/p3b-right3000', args: ['--zoe', '--course', 'PEAK3', '--seed-limit', '--seed-idle'], exactThrough: END, bonesThrough: 0, scoreThrough: 0, why: 'Peak 3 free ride into the EBC3_E ice blocks: repeated soft collisions (control 3) as on the PS2' },
  { name: 'course-limits/gs-zig3000', args: ['--zoe', '--course', 'PEAK1', '--peak-fresh'], exactThrough: END, bonesThrough: 0, scoreThrough: 0, why: 'CTM world start at Green Station (last lodge), zigzag for 3000 ticks: station fences, walls, crashes' },
  // Streamed Peak 2 world (docs/peak2.md section 6): free ride from Red Station (seed local/ps2-capture/peak2/frd-1800, region D)
  // down D -> D_DRA4 -> DRA4 (course 20 -> 3 at 3134, residency rows from the capture). Physics and bones exact to the end;
  // the trick score to the end: Conquer the Mountain free ride pays tricks as cash (2371: a 690-point trick is $1 through
// 119EF8, popup 0x2F, slot 0x19 = the cash; web/score_gameplay.inc); the 3142 score +0x168 tick after the 3134 course switch is
// the 117C28 score tick while a crossing has no path bank (docs/peak3.md "Past the crash contacts" 4).
  { name: 'peak2/fr-d-glide', args: ['--zoe', '--course', 'PEAK2'], exactThrough: END, scoreThrough: END, why: 'Peak 2 free ride from Red Station across D -> D_DRA4 -> DRA4 streaming, trick cash' },
  // The same line re-captured with weather watches (docs/weather.md section 11): the human's Weather painter (the record of the
  // region track gp+0x770: D, then D_DRA4 from the 2842 crossing, the switch on the PS2's tick) exact on every tick with its
  // distance through the 1916 crash (+0x460 kept at the crash body's last contact); the camera's painter values (its eye is not
  // seeded in free ride, so its distance is not gated); the flag manager built with the world: its 1 s wind timer on every tick
  // and the first wind draw (1772, mode 2 of the Peak 2 course table row) with the synced stream; later draws follow the
  // rider-FX stream (docs/weather.md 6). frd-regions.visual-state.json = the baseline's weather (tools/export_weather.py --state).
  { name: 'weather/frd-regions', args: ['--zoe', '--course', 'PEAK2', '--sync-visual-rng', '--weather', '../local/ps2-capture/runs/weather/frd-regions.weather-map.json'], exactThrough: END, scoreThrough: END,
    weatherExact: /^(rider\.|camera\.cur|flags\.timer)/, weatherFields: [[/^flags\.(wind|base|delta)$/, 1831]], why: 'Peak 2 free ride: Weather records by region track, flag manager wind' },
  // gravitude-event-tuck's line re-captured with weather watches: four human resets (1139, 1829, 2325, 2965 requests), each with
  // the reset fade's painter resets 0x2E47E8 on three renders (-99999 in both painters' +0 at records 1158-1160, 1848-1850,
  // 2344-2346, 2984-2986), the grid contact rider+0x460 of the ready state, crash contacts and placements as +0x460; the rider
  // painter exact through 1938 (1939: every painter jumps on the PS2 -- a computer rider's reset placement, 0x111890 of any rider;
  // this single-rider comparer runs no computer riders). The camera painter differs from the countdown (event camera, open).
  { name: 'weather/era5-reset', args: ['--zoe', '--event', '--weather', '../local/ps2-capture/runs/weather/era5-reset.weather-map.json'], stageWorld: true, exactThrough: END,
    weatherExact: /^rider\./, weatherThrough: 1938, why: 'Gravitude resets: fade painter resets, painter point' },
  // Kick Doubt (ESS3) through its Weather payload 1 (snowfall 0.68, lightning chance 0.02; the far east of the course): the
  // tuck line + an autopilot, with the time limit poked to 4:00 (GMM+0x78 = 14400, --time-limit; 60 s + the checkpoints cannot
  // reach it). Payload 1 from 6922; the human's own reset fade at 7399-7401; the rider painter exact until the opponent's reset
  // placement re-seeds it (6617; 0x111890 of any rider, not modelled in a single-rider comparison); the camera painter's lightning
  // chance (property 10, read by 0x2F00A0 for ScreenTint) until the second re-seed (7707). ess3-lightning: the same line, then
  // standing still in payload 1 for 6000 ticks; with the synced stream the PS2's strikes at 8581 (126396 cm, thunder 228 ticks)
  // and 12491 (27796 cm, 50) are reproduced; 8266 is not (the PS2 draws 6 words before the lightning draw that tick, the port 3:
  // the rider-FX stream gap, docs/weather.md 6).
  { name: 'weather/ess3-weather', args: ['--zoe', '--event', '--time-limit', '14400', '--weather', '../local/ps2-capture/runs/weather/ess3-weather.weather-map.json'], stageWorld: true, exactThrough: END, scoreThrough: 9861, boostThrough: 9861,
    weatherExact: /^rider\./, weatherThrough: 6616, weatherFields: [[/^camera\.cur10$/, 7706]], why: 'Kick Doubt weather area: painters, lightning chance' },
  { name: 'weather/ess3-lightning', args: ['--zoe', '--event', '--time-limit', '14400', '--sync-visual-rng', '--weather', '../local/ps2-capture/runs/weather/ess3-lightning.weather-map.json'], stageWorld: true, exactThrough: END,
    weatherExact: /^rider\./, weatherThrough: 6616, weatherFields: [[/^camera\.cur10$/, 7706], [/^lightning\./, 8265]], why: 'Kick Doubt lightning: strike draws against chance^2' },
  // Peak 1 Race from the objectives card's Continue (kind 5, mode 6; no countdown): the ABC1 grid slot 0 is the Happiness rival
  // rolling start (11DE60(rider, 0, 1) -> 11D660 semantic 5, 11DF18 forward x 833.333, z 0), but the run's setup had placed the
  // rider there and run frames before Continue: --peak-run seeds +0x2E4 3333.33 (11B3F8 motion 1), +0x380 = +0x370 and the
  // route words +0x490..+0x4CC of those frames (the heading +0x4CC is 13C948's fall line: the rival ready state's differs by
  // 3.5e-6 rad, which moved tick 1 by 6e-5 cm/s). Bones exact to the end since the rolling start runs the Continue's 11D390
  // placement (web/start_gameplay.inc; they were the rolling-start seed's, off from tick 1). The camera is not seeded (the
  // pre-start frames' history).
  { name: 'peak1-race-start', args: ['--zoe', '--course', 'ABC1', '--event', '--peak-run'], exactThrough: END, why: 'Peak 1 Race start: Cross on Continue (ollie on tick 0), straight tuck down ABC1 for 1600 ticks' },
  // Free-ride Transport arrivals (docs/peak-mountain.md "Captures"; neutral pad from the screen-10 state): --peak-arrival starts
  // at the placement record (after control 13) and places with core place_rider_region = 11D390 -> 11DE60 -> 11D660 semantic 5
  // clearance 0 -> 11DF18 (route attached at the grid slot by 112180 first; courses < 14 session point 1, stations/backcountry
  // their rows). Physics, score and boost exact to the end, body bones 0..23 too; the hair springs (24..28) carry the transport
  // pre-state the baseline does not hold (reported, not gated). The camera is not seeded.
  ...['ass1', 'aba1', 'bra2', 'bhp1'].map((l) => ({ name: `peak1-arrive-${l}`, args: ['--zoe', '--course', 'PEAK1', '--peak-arrival'], exactThrough: END, bonesThrough: 0, why: `Peak 1 free-ride Transport arrival at ${l.toUpperCase()}` })),
  // Metro-City arrivals with the stick up (local/ctm-events/caps, from bra2-screen10; docs/core-gameplay-fidelity.md "Speed limit"):
  // the tuck from the placement tick (13C948 on the carried route heading +0x4CC) and from 2070 (11B3F8's +0x2E4 on the tuck onset;
  // the 2280 path switch at 1371 cm off the route, 112338's tick % 60 on the rider manager's +8).
  ...['late', 'early'].map((n) => ({ name: `ctm-events/bra2-tuck-${n}`, args: ['--zoe', '--course', 'PEAK1', '--peak-arrival'], exactThrough: END, bonesThrough: 0, why: `Metro-City Transport arrival, tuck ${n === 'late' ? 'from 2070' : 'from the placement'}` })),
  // A world start's fresh rider (docs/peak-mountain.md "Fresh rider at a world start" / "Station fences"): the CTM last-lodge start at
  // Green Station (ctm-parity/states/start-lodge -> world load, state before tick 0 = local/ps2-capture/peak1/green-start-t0.p2s),
  // neutral pad: through the station's peak-race fence line (the invisible wall before pv stationFences, tick 230) into the lodge
  // door (394: world state 14, the walk-in, which the comparer does not run). Hair bones and the cash HUD slot are not seeded.
  { name: 'peak1-green-start', args: ['--zoe', '--course', 'PEAK1', '--peak-fresh'], exactThrough: 393, bonesThrough: 0, scoreThrough: 0, why: 'Peak 1 free ride from Green Station (fresh rider, builtin 108 fences) into the lodge door' },
  // Bought attributes reach the ride (docs/career-events.md "Buy Attributes"): the lodge (lodge/attrs/lodge-rich: Zoe $20,000, Spin 10.8,
  // Toughness 11.0, Stability 1.8) buys Acceleration / Speed / Stability to 2.0 and Spin to 11.0 through the popup's Yes (0x150C20 x 12,
  // cash 12,250), Return to Game (Save progress? No) -> the world load -> neutral pad. The runtime bank 0x535538 holds 10,10,5,5,55,55,10;
  // the stat getters read int(raw/5)/11, so the speed limit rises from tick 0 (2207.1 -> 2219.7). Without --attributes the port leaves it
  // at tick 1 (0.017 cm); with them exact to the lodge walk-in at 391 (the faster rider reaches the door 3 ticks sooner).
  { name: 'peak1-lodge-attrs', args: ['--zoe', '--course', 'PEAK1', '--peak-fresh', '--attributes', '10,10,5,5,55,55,10'], exactThrough: 390, bonesThrough: 0, scoreThrough: 0, why: 'lodge Buy Attributes (Speed/Accel/Stability 2.0, Spin 11.0), Return to Game, the world-load ride from Green Station' },
  { name: 'pipe-run-event', args: ['--zoe', '--event'], exactThrough: END, why: 'tucked run with charged airs, spins, grabs, soft collisions and a crash; 2076 the rider touches mdl_BHP1_Reset_Plane_38: its stage handler (slot 2, program 27: builtin27 type 5 -> 10F1C0 -> 116120(rider,0,4), "Wrong Way!", control 9 from 2077; web/stage_script_gameplay.inc); finish control 10 entry 12C5C8 (119368: +0x120) at 2213' },
  // Speedrun techniques (docs/tricks-scoring.md "Speedrun techniques"; glide baseline, 16 KiB records with the score object).
  { name: 'tech-spin-k1', args: ['--zoe'], exactThrough: END, why: 'spin timing: D-pad spin pressed k ticks after takeoff (117FE0 spin dial, 12F620 rates), release, landing' },
  { name: 'tech-spin-k3', args: ['--zoe'], exactThrough: END, why: 'spin timing: D-pad spin pressed k ticks after takeoff (117FE0 spin dial, 12F620 rates), release, landing' },
  { name: 'tech-spin-k6', args: ['--zoe'], exactThrough: END, why: 'spin timing: D-pad spin pressed k ticks after takeoff (117FE0 spin dial, 12F620 rates), release, landing' },
  { name: 'tech-spin-k10', args: ['--zoe'], exactThrough: END, why: 'spin timing: D-pad spin pressed k ticks after takeoff (117FE0 spin dial, 12F620 rates), release, landing' },
  { name: 'tech-spin-k15', args: ['--zoe'], exactThrough: END, why: 'spin timing: D-pad spin pressed k ticks after takeoff (117FE0 spin dial, 12F620 rates), release, landing' },
  { name: 'tech-spin-k20', args: ['--zoe'], exactThrough: END, why: 'spin timing: D-pad spin pressed k ticks after takeoff (117FE0 spin dial, 12F620 rates), release, landing' },
  { name: 'tech-spin-k25', args: ['--zoe'], exactThrough: END, why: 'spin timing: D-pad spin pressed k ticks after takeoff (117FE0 spin dial, 12F620 rates), release, landing' },
  { name: 'tech-spin-k30', args: ['--zoe'], exactThrough: END, why: 'spin timing: D-pad spin pressed k ticks after takeoff (117FE0 spin dial, 12F620 rates), release, landing' },
  { name: 'tech-spin-right-k8', args: ['--zoe'], exactThrough: END, why: 'spin timing: D-pad spin pressed k ticks after takeoff (117FE0 spin dial, 12F620 rates), release, landing' },
  { name: 'tech-spin-long', args: ['--zoe'], exactThrough: END, why: 'spin timing: D-pad spin pressed k ticks after takeoff (117FE0 spin dial, 12F620 rates), release, landing' },
  { name: 'tech-spin-max-k6', args: ['--zoe'], exactThrough: END, why: 'spin timing with maxed stats (rider+0xB34 level 11 poke)' },
  { name: 'tech-spin-max-long', args: ['--zoe'], exactThrough: END, why: 'spin timing with maxed stats (rider+0xB34 level 11 poke)' },
  { name: 'tech-prewind-full', args: ['--zoe'], exactThrough: END, why: 'prewind buffer: D-pad held with Cross before the takeoff (12E9B8 prewind targets), spin/flip carried into the air' },
  { name: 'tech-prewind-late', args: ['--zoe'], exactThrough: END, why: 'prewind buffer: D-pad held with Cross before the takeoff (12E9B8 prewind targets), spin/flip carried into the air' },
  { name: 'tech-prewind-flip', args: ['--zoe'], exactThrough: END, why: 'prewind buffer: D-pad held with Cross before the takeoff (12E9B8 prewind targets), spin/flip carried into the air' },
  { name: 'tech-reprewind', args: ['--zoe'], exactThrough: END, why: 're-prewind: Cross held through the landing (131620 entry tick), prewind from a spin landing' },
  { name: 'tech-reprewind-late', args: ['--zoe'], exactThrough: END, why: 're-prewind: Cross held through the landing (131620 entry tick), prewind from a spin landing' },
  { name: 'tech-reprewind-spinland', args: ['--zoe'], exactThrough: END, why: 're-prewind: Cross held through the landing (131620 entry tick), prewind from a spin landing' },
  { name: 'tech-sb-opp', args: ['--zoe'], exactThrough: END, why: 'spinboosts: D-pad spin + opposite/same stick on groomed and thick snow (114CC0 reverse turn, 12E9B8 branches)' },
  { name: 'tech-sb-same', args: ['--zoe'], exactThrough: END, why: 'spinboosts: D-pad spin + opposite/same stick on groomed and thick snow (114CC0 reverse turn, 12E9B8 branches)' },
  { name: 'tech-sb-half', args: ['--zoe'], exactThrough: END, why: 'spinboosts: D-pad spin + opposite/same stick on groomed and thick snow (114CC0 reverse turn, 12E9B8 branches)' },
  { name: 'tech-sb-540', args: ['--zoe'], exactThrough: END, why: 'spinboosts: D-pad spin + opposite/same stick on groomed and thick snow (114CC0 reverse turn, 12E9B8 branches)' },
  { name: 'tech-sb-early', args: ['--zoe'], exactThrough: END, why: 'spinboosts: D-pad spin + opposite/same stick on groomed and thick snow (114CC0 reverse turn, 12E9B8 branches)' },
  { name: 'tech-sb-right', args: ['--zoe'], exactThrough: END, why: 'spinboosts: D-pad spin + opposite/same stick on groomed and thick snow (114CC0 reverse turn, 12E9B8 branches)' },
  { name: 'tech-sb-partial', args: ['--zoe'], exactThrough: END, why: 'spinboosts: D-pad spin + opposite/same stick on groomed and thick snow (114CC0 reverse turn, 12E9B8 branches)' },
  { name: 'tech-sb-reprewind', args: ['--zoe'], exactThrough: END, why: 'spinboosts: D-pad spin + opposite/same stick on groomed and thick snow (114CC0 reverse turn, 12E9B8 branches)' },
  { name: 'tech-sb-max', args: ['--zoe'], exactThrough: END, why: 'spinboost with maxed stats' },
  { name: 'tech-speedcap-groomed', args: ['--zoe'], exactThrough: END, why: 'speed caps: groomed/thick/ice speed-limit table (+24 max stat, switch -2%), boost pokes, level override 0x1470CD4' },
  { name: 'tech-speedcap-tuck', args: ['--zoe'], exactThrough: END, why: 'speed caps: groomed/thick/ice speed-limit table (+24 max stat, switch -2%), boost pokes, level override 0x1470CD4' },
  { name: 'tech-speedcap-switch', args: ['--zoe'], exactThrough: END, why: 'speed caps: groomed/thick/ice speed-limit table (+24 max stat, switch -2%), boost pokes, level override 0x1470CD4' },
  { name: 'tech-speedcap-level1', args: ['--zoe'], exactThrough: END, why: 'speed caps: groomed/thick/ice speed-limit table (+24 max stat, switch -2%), boost pokes, level override 0x1470CD4' },
  { name: 'tech-land-short', args: ['--zoe'], exactThrough: END, why: 'landers: airtime below 40 / about 60 / long, holding Cross or a tail/nose press through the touchdown' },
  { name: 'tech-land-short-cross', args: ['--zoe'], exactThrough: END, why: 'landers: airtime below 40 / about 60 / long, holding Cross or a tail/nose press through the touchdown' },
  { name: 'tech-land-short-crosslong', args: ['--zoe'], exactThrough: END, why: 'landers: airtime below 40 / about 60 / long, holding Cross or a tail/nose press through the touchdown' },
  { name: 'tech-land-short-tail', args: ['--zoe'], exactThrough: END, why: 'landers: airtime below 40 / about 60 / long, holding Cross or a tail/nose press through the touchdown' },
  { name: 'tech-land-short-nose', args: ['--zoe'], exactThrough: END, why: 'landers: airtime below 40 / about 60 / long, holding Cross or a tail/nose press through the touchdown' },
  { name: 'tech-land-mid', args: ['--zoe'], exactThrough: END, why: 'landers: airtime below 40 / about 60 / long, holding Cross or a tail/nose press through the touchdown' },
  { name: 'tech-land-mid-cross', args: ['--zoe'], exactThrough: END, why: 'landers: airtime below 40 / about 60 / long, holding Cross or a tail/nose press through the touchdown' },
  { name: 'tech-land-mid-crosslong', args: ['--zoe'], exactThrough: END, why: 'landers: airtime below 40 / about 60 / long, holding Cross or a tail/nose press through the touchdown' },
  { name: 'tech-land-mid-tail', args: ['--zoe'], exactThrough: END, why: 'landers: airtime below 40 / about 60 / long, holding Cross or a tail/nose press through the touchdown' },
  { name: 'tech-land-mid-nose', args: ['--zoe'], exactThrough: END, why: 'landers: airtime below 40 / about 60 / long, holding Cross or a tail/nose press through the touchdown' },
  { name: 'tech-land-long', args: ['--zoe'], exactThrough: END, why: 'landers: airtime below 40 / about 60 / long, holding Cross or a tail/nose press through the touchdown' },
  { name: 'tech-land-long-cross', args: ['--zoe'], exactThrough: END, why: 'landers: airtime below 40 / about 60 / long, holding Cross or a tail/nose press through the touchdown' },
  { name: 'tech-land-long-crosslong', args: ['--zoe'], exactThrough: END, why: 'landers: airtime below 40 / about 60 / long, holding Cross or a tail/nose press through the touchdown' },
  { name: 'tech-land-long-tail', args: ['--zoe'], exactThrough: END, why: 'landers: airtime below 40 / about 60 / long, holding Cross or a tail/nose press through the touchdown' },
  { name: 'tech-land-long-nose', args: ['--zoe'], exactThrough: END, why: 'landers: airtime below 40 / about 60 / long, holding Cross or a tail/nose press through the touchdown' },
  { name: 'tech-fc-grab-early', args: ['--zoe'], exactThrough: END, why: 'frame cancelling: grab/tweak released k ticks before landing, taps and holds (grab lifecycle, classes 18..21 crash rule)' },
  { name: 'tech-fc-grab-rel4', args: ['--zoe'], exactThrough: END, why: 'frame cancelling: grab/tweak released k ticks before landing, taps and holds (grab lifecycle, classes 18..21 crash rule)' },
  { name: 'tech-fc-grab-rel8', args: ['--zoe'], exactThrough: END, why: 'frame cancelling: grab/tweak released k ticks before landing, taps and holds (grab lifecycle, classes 18..21 crash rule)' },
  { name: 'tech-fc-grab-rel12', args: ['--zoe'], exactThrough: END, why: 'frame cancelling: grab/tweak released k ticks before landing, taps and holds (grab lifecycle, classes 18..21 crash rule)' },
  { name: 'tech-fc-grab-rel16', args: ['--zoe'], exactThrough: END, why: 'frame cancelling: grab/tweak released k ticks before landing, taps and holds (grab lifecycle, classes 18..21 crash rule)' },
  { name: 'tech-fc-grab-rel20', args: ['--zoe'], exactThrough: END, why: 'frame cancelling: grab/tweak released k ticks before landing, taps and holds (grab lifecycle, classes 18..21 crash rule)' },
  { name: 'tech-fc-grab-rel26', args: ['--zoe'], exactThrough: END, why: 'frame cancelling: grab/tweak released k ticks before landing, taps and holds (grab lifecycle, classes 18..21 crash rule)' },
  { name: 'tech-fc-grab-tap2', args: ['--zoe'], exactThrough: END, why: 'frame cancelling: grab/tweak released k ticks before landing, taps and holds (grab lifecycle, classes 18..21 crash rule)' },
  { name: 'tech-fc-grab-tap4', args: ['--zoe'], exactThrough: END, why: 'frame cancelling: grab/tweak released k ticks before landing, taps and holds (grab lifecycle, classes 18..21 crash rule)' },
  { name: 'tech-fc-grab-tap6', args: ['--zoe'], exactThrough: END, why: 'frame cancelling: grab/tweak released k ticks before landing, taps and holds (grab lifecycle, classes 18..21 crash rule)' },
  { name: 'tech-fc-grab-hold', args: ['--zoe'], exactThrough: END, why: 'frame cancelling: grab/tweak released k ticks before landing, taps and holds (grab lifecycle, classes 18..21 crash rule)' },
  { name: 'tech-fc-tweak-rel8', args: ['--zoe'], exactThrough: END, why: 'frame cancelling: grab/tweak released k ticks before landing, taps and holds (grab lifecycle, classes 18..21 crash rule)' },
  // Uber tricks on the Junction pipe with the sound/speech call log (tools/ps2_audio_log.py, docs/tricks-scoring.md "Uber tricks"):
  { name: 'uber-chain', args: ['--zoe'], exactThrough: END, audio: true, why: 'Tricky (poked meter, tier 1): four Ubers in consecutive airs (Can Opener, Counter Point, Mahogany, Springer), tier 1 -> 5 at each landing (10E9B4), the 4th crosses run Uber 4 (29B430 Arcade_Uber 1), combo payout' },
  { name: 'uber-bail', args: ['--zoe'], exactThrough: END, audio: true, why: 'pipe-uber pad script (400 ticks) with the call log: Tricky prompt, meter fill ticks, grab 0x77, points ticks, the Uber crash (bail -0.25, Tricky end 0x69 from 2997B8, pending Uber stop), crash / slide sounds, reset 0x7B' },
  { name: 'uber-multi', args: ['--zoe'], exactThrough: END, audio: true, why: 'max stats (rider+0xB34 = 11): the speedrun chain, two Ubers in one air (Can Opener To Late Counter Point, tier 1 -> 3), then Mahogany To Late Springer after a Square double tap between them (tier 3 -> 5)' },
  { name: 'uber-super-expire', args: ['--zoe'], exactThrough: END, audio: true, why: 'poked Super Uber (tier 10, 2 s left): expiry -> tier 5 + 20 s Tricky (boost_control), Tricky countdown ticks 0x68 (29AB40 per frame), a tier-5 Uber (second table) -> tier 6' },
  { name: 'uber-super', args: ['--zoe'], exactThrough: END, scoreThrough: 1746, audioThrough: 1745, audio: true, why: 'poked tier 9: an Uber -> tier 10 Super Uber (60 s, meter locked, 29B738 Arcade_Uber 2), then three Super Ubers (+0x58); 1746 a BHP1 pointa pickup (sound 0x76, +2000: stage builtin27 type 6, world agent) is not ported' },
  // Monster tricks (docs/tricks-scoring.md "Monster tricks"; tier 5 + max stats, Zoe's default tier >= 5 Ubers):
  { name: 'monster-housecat', args: ['--zoe'], exactThrough: END, audio: true, why: 'Da Housecat (FS 540 G-Money, 11B1A8 id 1): +10000 (acc14 += 1.0), HUD 0x32, name = monster name, 29B7E0 Arcade_Uber 8; then BS 540 G-Money (no monster)' },
  { name: 'monster-chembro', args: ['--zoe'], exactThrough: END, audio: true, why: 'Chemical Brother (Double Back Flip Bar Hop To Late Mute, +20000): a late plain grab after the Uber' },
  { name: 'monster-finger11', args: ['--zoe'], exactThrough: END, audio: true, why: 'Finger 11 (BS Back Flip 360 Slinger, +20000): diagonal D-pad flip + spin, R1+R2 Uber' },
  { name: 'monster-stoneage', args: ['--zoe'], exactThrough: END, audio: true, why: 'Stoneage (BS Double Back Flip 180 Madonna, +20000) with the lodge selection poked: Zoe slot 4 tier >= 5 row 2 (0x5316D1 = 2, table 0x45AEB8; compare-ps2-capture.mjs applies the row)' },
  // Crow's Nest (ABA1) kickers from the countdown anchor (--event, stage world), tier 5 + max stats (rider 0x1455E00):
  { name: 'monster-yellowcard', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, audio: true, why: 'Yellowcard (BS 900 Mattrickulater, +10000) off the first kicker; countdown beeps 0x4E (remaining % 60, and 0 with GO)' },
  { name: 'monster-deepsky', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, audio: true, why: 'Deepsky (FS 900 Indian To Late Method, +10000): Indian Uber then a late plain Method' },
  { name: 'monster-swollen', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, audio: true, why: 'Swollen Member (BS 720 Torpedo, +10000): Zoe slot 2 tier >= 5 row poked to Torpedo (0x5316C4 word)' },
  { name: 'monster-xexec', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, audio: true, why: 'X-Executioner (FS Triple Back Flip 180 SSXorcist, +30000) off the second kicker (244-tick air); slot 1 row poked to SSXorcist (0x5316BC word)' },
  { name: 'monster-repeat', args: ['--zoe'], exactThrough: END, audio: true, why: 'score history poked with the Da Housecat identity {0, 1 << 27} (0x5DCAAC): the monster is a repeat (1190F0), its points are halved (7020 vs 14040) but the bonus popup and speech still fire' },
  { name: 'monster-smurphy', args: ['--zoe'], exactThrough: END, audio: true, why: 'history poked as monster-repeat; Da Housecat followed by a late back flip: the flip joins the same identity (FS Rodeo G-Money), no monster and no repeat' },
  { name: 'trick-stall', args: ['--zoe'], exactThrough: END, audio: true, why: 'stall bonus on the pipe: back flip then the opposite D-pad for 60 ticks -> STALLED! (117FE0 slot 0xE, b = 5 on the flip axis, +0x1C += b/1200 per tick, paid at landing by 117908: Back Flip 4130 vs 2740 for a 30-tick stall)' },
  { name: 'tech-fc-uber-restart', args: ['--zoe'], exactThrough: END, why: 'Uber frame cancels (poked full meter): restart, tap, late release' },
  { name: 'tech-fc-uber-tap', args: ['--zoe'], exactThrough: END, why: 'Uber frame cancels (poked full meter): restart, tap, late release' },
  { name: 'tech-fc-uber-late', args: ['--zoe'], exactThrough: END, why: 'Uber frame cancels (poked full meter): restart, tap, late release' },
  { name: 'tech-railglitch-2', args: ['--zoe'], exactThrough: END, why: 'rail glitch: rail attach/leave k ticks apart, presses and jumps on the rail, air attach' },
  { name: 'tech-railglitch-4', args: ['--zoe'], exactThrough: END, why: 'rail glitch: rail attach/leave k ticks apart, presses and jumps on the rail, air attach' },
  { name: 'tech-railglitch-8', args: ['--zoe'], exactThrough: END, why: 'rail glitch: rail attach/leave k ticks apart, presses and jumps on the rail, air attach' },
  { name: 'tech-railglitch-nose', args: ['--zoe'], exactThrough: END, why: 'rail glitch: rail attach/leave k ticks apart, presses and jumps on the rail, air attach' },
  { name: 'tech-railglitch-tail', args: ['--zoe'], exactThrough: END, why: 'rail glitch: rail attach/leave k ticks apart, presses and jumps on the rail, air attach' },
  { name: 'tech-railglitch-jump', args: ['--zoe'], exactThrough: END, why: 'rail glitch: rail attach/leave k ticks apart, presses and jumps on the rail, air attach' },
  { name: 'tech-railglitch-air', args: ['--zoe'], exactThrough: END, why: 'rail glitch: rail attach/leave k ticks apart, presses and jumps on the rail, air attach' },
  { name: 'tech-strong-jump', args: ['--zoe'], exactThrough: END, why: 'strong stance: stance-dependent takeoffs, passive flights, collisions' },
  { name: 'tech-strong-right', args: ['--zoe'], exactThrough: END, why: 'strong stance: stance-dependent takeoffs, passive flights, collisions' },
  { name: 'tech-strong-passive', args: ['--zoe'], exactThrough: END, why: 'strong stance: stance-dependent takeoffs, passive flights, collisions' },
  { name: 'tech-strong-collide', args: ['--zoe'], exactThrough: END, why: 'strong stance: stance-dependent takeoffs, passive flights, collisions' },
  { name: 'tech-freefall-tail', args: ['--zoe'], exactThrough: END, why: 'free-fall slide: tail press held from the air into the slide, spin' },
  { name: 'tech-freefall-tail-back', args: ['--zoe'], exactThrough: END, why: 'free-fall slide: tail press held from the air into the slide, spin' },
  { name: 'tech-freefall-spin', args: ['--zoe'], exactThrough: END, why: 'free-fall slide: tail press held from the air into the slide, spin' },
  { name: 'tech-trip-left', args: ['--zoe'], exactThrough: END, why: 'trip turn: hard turns to a stop (114CC0), both directions' },
  { name: 'tech-trip-right', args: ['--zoe'], exactThrough: END, why: 'trip turn: hard turns to a stop (114CC0), both directions' },
  { name: 'tech-trip-stop', args: ['--zoe'], exactThrough: END, why: 'trip turn: hard turns to a stop (114CC0), both directions' },
  { name: 'tech-select-ground', args: ['--zoe'], exactThrough: END, why: 'select warp: Select (ResetPath 0x1000 -> 116120) on the ground, in the air, in a crash, on a rail, twice' },
  { name: 'tech-select-air', args: ['--zoe'], exactThrough: END, why: 'select warp: Select (ResetPath 0x1000 -> 116120) on the ground, in the air, in a crash, on a rail, twice' },
  { name: 'tech-select-crash', args: ['--zoe'], exactThrough: END, why: 'select warp: Select (ResetPath 0x1000 -> 116120) on the ground, in the air, in a crash, on a rail, twice' },
  { name: 'tech-select-rail', args: ['--zoe'], exactThrough: END, why: 'select warp: Select (ResetPath 0x1000 -> 116120) on the ground, in the air, in a crash, on a rail, twice' },
  { name: 'tech-select-twice', args: ['--zoe'], exactThrough: END, why: 'select warp: Select (ResetPath 0x1000 -> 116120) on the ground, in the air, in a crash, on a rail, twice' },
  { name: 'tech-oob-neutral', args: ['--zoe'], exactThrough: END, why: 'out of bounds: a long neutral run off the course' },
  { name: 'tech-oob-dance', args: ['--zoe'], exactThrough: END, why: 'stance dancing out of bounds: 1210B0 direction-change / crash-air bounce timers request 116120 reason 2 (motion 3 from the request tick, 12F398 first steps next tick, a repeated request restarts the progress)' },
  { name: 'tech-oob-hops', args: ['--zoe'], exactThrough: END, why: 'stance dancing out of bounds: 1210B0 direction-change / crash-air bounce timers request 116120 reason 2 (motion 3 from the request tick, 12F398 first steps next tick, a repeated request restarts the progress)' },
  { name: 'tech-bonus-tricks', args: ['--zoe'], exactThrough: END, why: 'finish-line trick bonus 1194C0 (poked checkpoint table 0x4D33B8, game mode, flag bit 9): Arcade_Bonus awards through 10E098' },
  { name: 'tech-bonus-racemode', args: ['--zoe'], exactThrough: END, why: 'finish-line trick bonus 1194C0 (poked checkpoint table 0x4D33B8, game mode, flag bit 9): Arcade_Bonus awards through 10E098' },
  { name: 'tech-bonus-disabled', args: ['--zoe'], exactThrough: END, why: 'finish-line trick bonus 1194C0 (poked checkpoint table 0x4D33B8, game mode, flag bit 9): Arcade_Bonus awards through 10E098' },
  { name: 'jump-tricks16', args: ['--zoe'], exactThrough: END, why: 'jump-tricks script with 16 KiB records (animation sequences seeded): bones end to end' },
  { name: 'passive-inputs16', args: ['--zoe'], exactThrough: END, why: 'passive-inputs script with 16 KiB records' },
  // Metro-City phone booths / water towers (stage builtin 34 -> 0x123210, web/stage_teleport.inc; docs/stage-teleport.md 4):
  // tools/ps2_booth_inject.py writes rider+0xA30 = a beam instance on the injected ticks, so 121818 runs the beam's program
  // as a real contact (one builtin 77 draw, then the teleport). The comparer injects the same contacts (booth_injections) and
  // turns the teleport on for these captures whatever pv boothTeleport says.
  { name: 'booth/teleports', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, scoreThrough: END, bonesThrough: END, boostThrough: END, why: 'booth 0004 x4 from the ground (0005 x3 and the 25 % 0006), booth 0007 -> 0008, the water tower from the air (control 5): physics, score, bones, boost and the human draw count exact every tick' },
  { name: 'booth/from-crash', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, scoreThrough: END, bonesThrough: END, boostThrough: END, why: 'booth 0007 while crashing (motion 2 / control 8)' },
  { name: 'booth/from-passive-air', args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, scoreThrough: END, bonesThrough: END, boostThrough: END, why: 'booth 0007 in passive air (control 4)' },
  { name: 'booth/from-rail', args: ['--zoe', '--event'], stageWorld: true, exactThrough: 2302, scoreThrough: 2302, bonesThrough: 2302, boostThrough: END, why: 'booth 0007 while grinding (motion 4 / control 7): the speed leaves the PS2 by 0.04 cm/s at 2303 (extra lean / board lift; docs/stage-teleport.md 4: rail exit, open)' },
  { name: 'setpieces/full', keepCheck: true, args: ['--zoe', '--event'], stageWorld: true, exactThrough: END, bonesThrough: 12368, scoreThrough: 12296, boostThrough: 12296, why: 'full Snow Jam run from the grid through the finish area with the stage world (the finish reset planes kill themselves in their section-enter program: builtin43 race != free ride -> builtin2 DeadNode); 12297 HUD slots 5/6 and the finish boost award (+0.35, also without the stage world: finish logic, physics agent); 12369 posed bones after the finish celebration' },
  // Camera parity (docs/CAMERA_RECOVERY.md): --watch <camera>:0x390 and the visual RNG 0x4FF018.
  { name: 'cam-event-start', args: ['--zoe', '--event'], exactThrough: END, cameraThrough: END, why: 'event-start with the DEFAULT_3 camera (0x390 bytes) and visual RNG watched: the countdown-anchor camera seed (game tick 18) and every camera word' },
  { name: 'cam-event-race', args: ['--zoe', '--event', '--camera-shake-sync'], exactThrough: END, cameraThrough: END, why: 'event-race camera words: landing crash 899 (136C40 predictor restart before the camera), crash shakes (walk adopted from the PS2 when the visual RNG stream differs)' },
  { name: 'cam-mix-glide', args: ['--zoe', '--camera-seed', '--camera-shake-sync'], exactThrough: END, cameraThrough: END, why: 'glide camera words seeded from record 0: boosts, carves, spins, crashes' },
  { name: 'cam-mix-glide-0x3C', args: ['--zoe', '--camera-seed', '--camera-shake-sync', '--camera-variant', '60'], exactThrough: END, cameraThrough: END, why: 'Near camera (DEFAULT_2 vtable 0x45CAB0)' },
  { name: 'cam-mix-glide-0x3E', args: ['--zoe', '--camera-seed', '--camera-shake-sync', '--camera-variant', '62'], exactThrough: END, cameraThrough: END, why: 'Far camera (DEFAULT_4 vtable 0x45C9C0)' },
  { name: 'cam-boost-slow', args: ['--zoe', '--camera-seed', '--camera-shake-sync'], exactThrough: END, cameraThrough: END, why: 'Snow Jam glide with the Tricky meter poked (rider +0x2F8 1, +0x2F0 20 s, +0x2F4 1), braked to 1.7 km/h, then Square: 15E460 requests shake 1..3 at scale 0 every tick and 15E360 sets +0x458 pending on every call that is not suppressed (0x15E444 delay slot), so the shake starts (12 visual-RNG draws) at amplitude 0 (649); the port set pending only when the amplitude grew' },
  { name: 'cam-air-tricks', args: ['--zoe', '--camera-seed', '--camera-shake-sync'], exactThrough: END, cameraThrough: END, why: 'air-tricks camera words: jump camera splines, landings, crash' },
  { name: 'cam-rail-balance-lr', args: ['--zoe', '--camera-seed', '--camera-shake-sync'], exactThrough: END, cameraThrough: END, why: 'rail camera (motion 4)' },
  { name: 'cam-pipe-air', args: ['--zoe', '--camera-seed', '--camera-shake-sync'], exactThrough: END, cameraThrough: END, why: 'super pipe camera; 793 crash contacts write the camera surface rider+0x438' },
  { name: 'cam-metro-mix-glide', args: ['--zoe', '--camera-seed', '--camera-shake-sync'], exactThrough: END, cameraThrough: END, why: 'Metro-City camera words' },
];
const cwd = new URL('.', import.meta.url).pathname;
const through = (v) => (v === undefined ? END : v);
// booth/*: a core with the booth-contact injection (web/stage_teleport.inc); an older live core skips them.
const coreJsText = fs.readFileSync(process.env.CORE_JS || new URL('runtime/core.js', import.meta.url), 'utf8'), coreHasBooth = coreJsText.includes('stage_contact_inject');
// c.coreHas: a glide seed the case needs (web/generate-controllers.py compiles local/assets/native/<X> into the core); an older core skips it.
const coreWasmText = (() => { try { return fs.readFileSync(process.env.CORE_JS ? process.env.CORE_JS.replace(/\.js$/, '.wasm') : new URL('runtime/core.wasm', import.meta.url)).toString('latin1'); } catch { return ''; } })();
const run1 = (c) => new Promise((resolve) => {
  const bin = runs + c.name + '.bin';
  if (!fs.existsSync(bin) || !fs.existsSync(runs + c.name + '.capture.json')) return resolve({ c, skip: true });
  if (c.name.startsWith('booth/') && !coreHasBooth) return resolve({ c, skip: true, why: 'the core predates stage_contact_inject (web/stage_teleport.inc)' });
  if (c.coreHas && !coreWasmText.includes(c.coreHas)) return resolve({ c, skip: true, why: `the core has no ${c.coreHas} seed` });
  const report = reportPath(c.name, 'regression.json');
  const env = { ...process.env, ...(c.boardPress ? { BP_ALL_SEMANTICS: '1' } : {}), ...(c.stageWorld ? { STAGE_WORLD: '1' } : {}), ...(c.audio ? { TICK_HOOK: 'uber-audio-compare.mjs' } : {}), ...(c.keepCheck && coreJsText.includes('_snapshot_save') ? { SNAPSHOT_KEEP_CHECK: '1' } : {}), BONE_SCAN: '1', BONE_SCAN_MAX: '29' }; // stageWorld: sections + stage programs as the browser (web/stage_world.inc)
  execFile(process.execPath, ['compare-ps2-capture.mjs', bin, '--pad', '--sync-rng', '--report', report, ...c.args, ...(c.boardPress ? ['--board-press'] : [])], { cwd, env, encoding: 'utf8', maxBuffer: 1 << 28 }, (error, _stdout, stderr) => resolve({ c, error, stderr, report }));
});
const check = ({ c, error, stderr, report }) => {
  if (error) throw new Error(`${c.name}: comparer failed: ${error.message}\n${(stderr || '').slice(-2000)}`);
  const exactThrough = through(c.exactThrough), bonesThrough = through(c.bonesThrough), scoreThrough = through(c.scoreThrough);
  const m = /^bones (\d+)/m.exec(stderr || ''); if (m && Number(m[1]) <= bonesThrough) throw new Error(`${c.name}: posed body bones left the original at tick ${m[1]} (baseline bit-exact through ${bonesThrough === END ? 'the end' : bonesThrough})`);
  const { summary, rows } = JSON.parse(fs.readFileSync(report, 'utf8'));
  // keepCheck (docs/replay.md §2a): the rider-context snapshot saved before the run and restored after it: its kept tables unchanged
  if (c.keepCheck && coreJsText.includes('_snapshot_save') && summary.snapshotKeepCheck !== 'ok') throw new Error(`${c.name}: ${summary.snapshotKeepCheck ?? 'no snapshot keep check in the report'}`);
  const firstBad = rows.find((r) => !r.exact);
  const firstBadTick = firstBad ? firstBad.tick : END;
  if (firstBadTick !== END && firstBadTick <= exactThrough) throw new Error(`${c.name}: browser left the original at tick ${firstBadTick} (baseline exact through ${exactThrough === END ? 'the end' : exactThrough}; ${c.why})`);
  if (c.within) { const worst = rows.filter((r) => r.tick <= c.within[0]).reduce((w, r) => Math.max(w, r.posErrCm), 0);
    if (worst > c.within[1]) throw new Error(`${c.name}: ${worst.toFixed(3)} cm error by tick ${c.within[0]} (limit ${c.within[1]})`); }
  if (c.rollerTicks !== undefined && (summary.firstRollerMismatch || summary.rollerTicksExact < c.rollerTicks)) throw new Error(`${c.name}: roller state left the original (${summary.rollerTicksExact} exact roller ticks, baseline ${c.rollerTicks}; first mismatch ${JSON.stringify(summary.firstRollerMismatch)})`);
  if (c.boardPress) for (const [key, v] of Object.entries(summary.fieldFirst || {})) if (key.startsWith('bp.') && v.tick <= Math.min(exactThrough, firstBadTick - 1)) throw new Error(`${c.name}: board-press ${key} differs at ${v.tick} (web ${v.web}, PS2 ${v.ps2})`);
  if (summary.firstWordMismatch && summary.firstWordMismatch.tick <= Math.min(exactThrough, firstBadTick - 1)) throw new Error(`${c.name}: command words differ at ${summary.firstWordMismatch.tick}`);
  // The retained speed limit rider +0x2E4 (11B3F8) while the physics are exact: the comparer reads record i's (the limit tick i ran
  // with) against the browser's after tick i. A start seed's limit may differ from the savestate's and converge through 11B3F8's
  // filter (bit-equal within ~130 ticks, before the ride): gated from the first equal tick, and it must become equal.
  { const through = Math.min(c.limitThrough ?? exactThrough, firstBadTick - 1), l = summary.firstSpeedLimitDivergence, first = summary.firstSpeedLimitMismatch;
    if (l && l.tick <= through) throw new Error(`${c.name}: speed limit +0x2E4 left the original at ${l.tick} (web ${l.web}, PS2 ${l.ps2})`);
    // (by row: a CTM in-world capture restarts the tick count at the Continue)
    const badRow = firstBad ? rows.indexOf(firstBad) : rows.length, agreedRow = summary.speedLimitAgreedRow ?? (summary.speedLimitAgreed === null ? null : rows.findIndex((r) => r.tick === summary.speedLimitAgreed));
    if (first && (agreedRow === null || agreedRow >= badRow)) throw new Error(`${c.name}: speed limit +0x2E4 never reached the original (first ${first.tick}: web ${first.web}, PS2 ${first.ps2})`); }
  if (c.lightingReferences !== undefined) { const l = summary.lighting; if (!l) throw new Error(`${c.name}: no lighting summary (pass --lighting)`);
    if (l.referenceTicks < c.lightingReferences) throw new Error(`${c.name}: Lighting bank reference left the PS2 at ${l.firstReference?.tick} (${l.referenceTicks} ticks match, baseline ${c.lightingReferences})`);
    if (l.firstScalar && l.firstScalar.tick <= c.lightingScalarsThrough) throw new Error(`${c.name}: Lighting scalars left the PS2 at ${l.firstScalar.tick} (baseline exact through ${c.lightingScalarsThrough})`); }
  // Score object + HUD slots: every capture that records them (16 KiB layouts).
  if (summary.scoreTicksExact !== undefined && summary.firstScoreMismatch !== null && summary.firstScoreMismatch !== undefined && summary.firstScoreMismatch <= scoreThrough) { const [key, v] = Object.entries(summary.scoreFirst)[0]; throw new Error(`${c.name}: trick score left the original at ${summary.firstScoreMismatch} (${key}: web ${v.web}, PS2 ${v.ps2}; baseline exact through ${scoreThrough === END ? 'the end' : scoreThrough})`); }
  if (c.scoreThrough !== undefined && summary.scoreTicksExact === undefined) throw new Error(`${c.name}: capture has no score layout (re-capture with tools/ps2_capture.py)`);
  // Boost / Tricky / Uber tier words (rider +0x2F8 meter, +0x2FC amount, +0x2F4 tier, +0x2F0 Tricky time) on every compared tick.
  if (summary.firstBoostMismatch && summary.firstBoostMismatch.tick <= Math.min(c.boostThrough ?? exactThrough, exactThrough, firstBadTick - 1)) throw new Error(`${c.name}: boost state left the original at ${summary.firstBoostMismatch.tick} (web ${JSON.stringify(summary.firstBoostMismatch.web)}, PS2 ${JSON.stringify(summary.firstBoostMismatch.ps2)})`);
  // Sound / speech dispatch (c.audio: web/uber-audio-compare.mjs against the capture's tools/ps2_audio_log.py call log).
  if (c.audio) { const a = summary.audio; if (!a) throw new Error(`${c.name}: no audio call log (capture with ps2_capture.py build --audio-log)`);
    const bad = (a.audioMismatches || []).find((x) => x.tick <= through(c.audioThrough ?? c.exactThrough));
    if (bad) throw new Error(`${c.name}: sound/speech dispatch differs at ${bad.tick} (web ${JSON.stringify(bad.web)}, PS2 ${JSON.stringify(bad.ps2)})`); }
  if (c.cameraThrough !== undefined) { const bad = summary.firstCameraWordMismatch; if (!summary.cameraWordTicks) throw new Error(`${c.name}: no camera words compared`);
    if (bad && bad.tick <= c.cameraThrough) throw new Error(`${c.name}: camera words left the original at ${bad.tick}: ${JSON.stringify(bad.words.slice(0, 4))}`); }
  // Weather state (compare-ps2-capture.mjs --weather MAP: the human's / camera's Weather painters, the flag manager wind): every
  // field matching c.weatherExact equal on every compared tick through c.weatherThrough (default: the physics span).
  if (c.weatherExact) { const w = summary.weather; if (!w) throw new Error(`${c.name}: no weather compare (--weather)`);
    const keys = Object.keys(w.ticks).filter((k) => c.weatherExact.test(k)); if (!keys.length) throw new Error(`${c.name}: no weather fields gated`);
    const limit = Math.min(through(c.weatherThrough ?? c.exactThrough), firstBadTick - 1);
    for (const k of keys) { const f = w.first[k]; if (f && f.tick <= limit) throw new Error(`${c.name}: weather ${k} left the original at ${f.tick} (web ${f.web}, PS2 ${f.ps2})`); } }
  for (const [re, until] of c.weatherFields || []) { const w = summary.weather, keys = Object.keys(w?.ticks || {}).filter((k) => re.test(k)); if (!keys.length) throw new Error(`${c.name}: no weather fields ${re}`);
    for (const k of keys) { const f = w.first[k]; if (f && f.tick <= until) throw new Error(`${c.name}: weather ${k} left the original at ${f.tick} (baseline exact through ${until})`); } }
  const notes = [c.audio && summary.audio ? `audio ${summary.audio.eventTicksExact}/${summary.audio.eventTicks} event ticks` : '', c.bonesThrough !== undefined ? `bones through ${c.bonesThrough}` : '', c.scoreThrough !== undefined ? `score through ${c.scoreThrough}` : '', c.cameraThrough !== undefined ? `camera ${summary.cameraWordTicksExact}/${summary.cameraWordTicks}` : ''].filter(Boolean).join(', ');
  return `${c.name}: exact through ${firstBadTick === END ? 'the end' : firstBadTick - 1} (${rows.length} ticks${notes ? '; ' + notes : ''})`;
};
let ran = 0;
const only = process.env.ONLY ? new Set(process.env.ONLY.split(',')) : null;
// A full pass on the live core stamps its sha256; deploy/deploy-staged.sh refuses a core without that stamp.
const coreHash = () => createHash('sha256').update(fs.readFileSync(new URL('runtime/core.wasm', import.meta.url))).digest('hex'), coreAtStart = coreHash();
// pending cases (a build stage's gate before its pv switch exists; docs/ctm-events-in-world.md): only with PENDING=1 or ONLY naming them.
const pendingRun = (c) => process.env.PENDING === '1' || (only && only.has(c.name));
for (const c of cases) if (c.pending && (!only || only.has(c.name)) && (c.unscored || !pendingRun(c))) console.log(`pending ${c.name} [pv ${c.pending}]: ${c.unscored ? 'unscored: ' + c.unscored : 'skipped (PENDING=1 scores it)'}`);
const selected = cases.filter((c) => (!only || only.has(c.name)) && !(c.pending && (c.unscored || !pendingRun(c))));
const parallel = Math.max(1, Number(process.env.PAR) || Math.min(12, (os.availableParallelism?.() ?? os.cpus().length) - 1));
const results = new Array(selected.length); let next = 0;
await Promise.all(Array.from({ length: Math.min(parallel, selected.length) }, async () => { while (next < selected.length) { const k = next++; results[k] = await run1(selected[k]); } }));
const failures = [];
for (const r of results) {
  if (r.skip) { console.log(`skip ${r.c.name}: ${r.why || 'capture not present'}`); continue; }
  if (r.c.pending) { let msg; try { msg = check(r); } catch (e) { msg = e.message; } console.log(`pending [pv ${r.c.pending}] ${msg} (today: ${r.c.today})`); continue; }
  try { console.log(check(r)); ran++; } catch (e) { failures.push(e.message); console.error('FAIL ' + e.message); }
}
if (failures.length) throw new Error(`${failures.length} capture gate(s) regressed:\n${failures.join('\n')}`);
// Six-rider races (web/compare-ai-capture.mjs, captures built with --ai-state): the five computer riders
// run the original NPC provider in their own cores, all six riders share one world and run in the
// original pass order (web/ai-racers.js: poses, then each 121750 with rider pairs 107888 inside it, then
// each 121818), and every shared-RNG draw comes from the six riders' code (no per-tick RNG copy; only
// the unported world-pass object 0x341AA0 draw is taken from the capture log, --world-draws).
// Baselines: human exact through, each computer rider's first inexact tick, shared RNG first mismatch,
// ranking (+0xEC) and the 10F560 pair records never differ.
const aiCases = [
  { name: 'event-race-ai', args: ['--zoe', '--isolate'], humanThrough: 2417, ai: [2417, 2417, 2417, 2417, 2417], rngThrough: 2417, ranks: true, records: true,
    why: 'event-race pad with all five computer riders: grid control 6 draws, push-offs, route switches, tricks, rails (1846 Psymon rail post 13C140 terrain contact), resets' },
  { name: 'ai-idle', args: ['--zoe', '--isolate'], humanThrough: 2617, ai: [2617, 2617, 2617, 2617, 2617], rngThrough: 2617, ranks: true, records: true,
    why: 'human idle on the grid while the five computer riders race (1549 Psymon passive landing keeps +2DC)' },
  // 2026-09-28: every rider, the RNG, the score, the ranks and the pair records exact to the end (367) once the comparer applies the
  // knockdown's relationship change (0x155BF0); was human 265, Moby 265, rider 5 314, RNG 308.
  { name: 'ko-attack', args: ['--zoe'], humanThrough: 367, ai: [367, 367, 367, 367, 367], rngThrough: 367, scoreThrough: 367, ranks: true, records: true,
    why: 'knockdown: a charged punch (closed-loop pad recorded in the browser) knocks Moby down at 264: 10E468 -> 119400 KO +0x128, popup 0x2C and 10E098(attacker, 1.0, 2) (score, HUD bank and boost words exact); 266 the post-knockdown pair separation of human and Moby lands 0.005 cm off (rider-pair physics, open)' },
  { name: 'event-race-ai-pairs', args: ['--zoe'], humanThrough: 2417, ai: [2417, 2417, 2417, 2417, 2417], rngThrough: 2417, ranks: true, records: true,
    why: 'event-race pad WITHOUT --isolate: human<->computer rider bump inside the human 121750 (0x107888, soft reactions 285), Luther landing crash 770 (13AA48 as a ragdoll), 115D48 rival flag +0x1C' },
  // Happiness (ABC1) Rival Time vs Mac from the rolling start (no countdown; docs/backcountry.md), not isolated. idle: Cross held
  // 10 ticks (control 2 on the departure tick), then neutral; 1032 is a soft collision with mdl_ABC1_treehevbump_0_000129, whose
  // contact LiveComp wobbles its collision (0x334888 on the entity's nodes; every rider exact to the end since 2026-09-26). tuck: bc-autopilot.mjs line; the landing at 5229 after an air soft
  // collision (control 3 -> 0, finish_landing keeps +2DC/prewind and ends the soft controller) is exact: all six riders through 6700.
  { name: 'bc/bc-race-idle', args: ['--zoe', '--ticks', '2400'], humanThrough: 2400, ai: [2400], rngThrough: 2400, ranks: true, records: true,
    why: 'Happiness Rival Time, human idle after the start ollie: rolling start, rival provider from tick 0, stage world (builtin77 world pass)' },
  { name: 'bc/bc-race-tuck', args: ['--zoe', '--ticks', '6700'], humanThrough: 6700, ai: [6700], rngThrough: 6700, ranks: true, records: true,
    why: 'Happiness Rival Time, tucked autopilot line: raven/osprey/tumbler spline pieces (359460 draws), section activation, DynamicParticle' },
  // 2026-09-28: the comparer tracks the in-race relationships (0x155BF0; web/ai-race.js pv rivalRelations): Mac's soft attacks raise his
  // record of Zoe to level 2 at 599, 10F560 flags the pair, and the human's 115D48 picks 319 at 1161. Was human 2008 / Mac 1546 / RNG 1300.
  { name: 'bc/bc-race-tuck2', args: ['--zoe', '--ticks', '2100'], humanThrough: 2100, ai: [2100], rngThrough: 2100, ranks: true, records: true,
    why: 'Happiness Rival Time, second autopilot line (PS2 pad decode): 1301 the human 115D48 idle clock reaches 1 s four ticks early (upper reaction clip after 1162)' },
  // Peak 3 (docs/peak3.md): The Throne (EBC3) Rival Time vs Psymon from the rolling start, not isolated: Cross held 10 ticks,
  // then neutral (as bc-race-idle); Psymon rides with the Peak 3 computer-rider stats (bank 2 raw 35).
  { name: 'peak3/the-throne-race-idle', args: ['--zoe', '--ticks', '2900'], humanThrough: 2900, ai: [2900], rngThrough: 2900, ranks: true, records: true,
    why: 'The Throne Rival Time, human idle after the start ollie: rolling start, the rival provider from tick 0, stage world; human, Psymon, RNG, ranks and pair records exact' },
  // 2026-09-28: Mac 873 (crashed by Nate's pair before his 121750: no 13F2E0 board normal) and 977 (a sliding crash bounce, 138640,
  // does not run 105D98) fixed: Nate / Allegra 1129 -> 1232 / 1273, Mac 1231 -> 1232, pair records 1134 -> 1236.
  // 2026-09-28 (later): the crash billboard (section LiveComp 332333 with a slot-5 timer program) is a core entity, and builtins
  // 28 / 54 act on it: the trigger plays it on and its program 85 breaks the ice pieces (2 draws at 1195, 1209, 1239: RNG blips).
  // Then: 1211F8 approaches turn / brake / crouch on the tick 0x132770 ends a rail's airborne control 7 (Allegra 3749): the human, Nate,
  // Mac and Luther exact to the end.
  // Then: control 2's 12E9B8 returns as soon as its 106848 attaches (no targets, no clip on the attach tick) and a held jump never
  // enters control 7 (131D08) on the rail (Moby 5136): Moby exact to the end.
  // Then: the crash controller 12CB68 begins with 114130(rider, 0, 0), so a crash stops the boost (+0x2FC 0; Allegra 4911): her get-up
  // tick's ground drive no longer adds the boost term (5054). All six riders, the RNG, the ranks and the pair records exact to the end.
  { name: 'peak3/gravitude-race-ai', args: ['--zoe', '--isolate'], humanThrough: 5999, ai: [5999, 5999, 5999, 5999, 5999], rngThrough: 5999, ranks: true, records: true,
    why: 'Gravitude six riders (Peak 3 stats, bank 2 raw 35): the computer riders ride exact past the start and the first jumps; 1505 the human landing soft collision (see the physics gate); 1129 Nate and Allegra (open); Moby / the fifth rider stay exact past the shared-RNG divergence (1195) only as long as the post-divergence draw order allows (1357 / 1478 since the human stays exact to 1504; 1736 / 2006 before)' },
  // Six-rider parity captures (tools/ps2_capture.py --ai-state, local/ps2-capture/runs/parity-ai), gated since 2026-09-28:
  //  * the computer riders' ground query walks the rider's scope list, refreshed per roster slot (120F20: tick % 3 == rider+0x86C % 3;
  //    11D660 placements rebuild it): metro Luther left at 506 (patch 75280 joins his list at 508), Moby 554, RNG 563;
  //  * a landing runs 115640 (139C88): a soft collision off a rail keeps stance 4 to the touchdown (ESS3 Moby 815);
  //  * the air body query filters with this tick's +0x180 (pose 121728 before motion 121750): ESS3 839, DSS2 Moby 1509 / RNG 1626,
  //    ARA1 human 3799, riders 3282/3587/3312/1644/2709, RNG 2404;
  //  * a 13F178 departure seeds the flight (13F2CC 11FE78(1) -> 1135B8) before the 13F358 clamp: ARA1 Luther 3799.
  { name: 'parity-ai/metro-race', args: ['--zoe', '--isolate'], humanThrough: 1199, ai: [1199, 1199, 1199, 1199, 1199], rngThrough: 1199, ranks: true, records: true, scoreThrough: 1199,
    why: 'Metro-City race, six riders to the end: scope-listed ground contact with per-slot refresh (was Luther 506, Moby 554, RNG 563, human score 1096)' },
  { name: 'parity-ai/ess3', args: ['--zoe', '--isolate'], humanThrough: 999, ai: [999], rngThrough: 999, ranks: true, records: true, scoreThrough: 999,
    why: 'ESS3 with Moby: rail, soft collision off the rail, landing stance restore, air slope contact at the 0.8 filter (was Moby 815, RNG 822)' },
  { name: 'parity-ai/ess3-long', args: ['--zoe', '--isolate'], humanThrough: 3699, ai: [3699], rngThrough: 3699, ranks: true, records: true, scoreThrough: 3699,
    why: 'Kick Doubt with Moby, 3700 ticks: his soft collision off a rail wall at 2943 runs the control-7 exit 132048 (+0x238 target 0, the 18..20 cycle blend fades 0.4636 -> 0.3969 -> 0.3303); the pair phase no longer restores the pre-reaction triplets (was Moby 2945, RNG 2978, human 3278; the HUD sweep saw his score differ by 3219)' },
  { name: 'parity-ai/dss2', args: ['--zoe', '--isolate'], humanThrough: 1799, ai: [1799], rngThrough: 1799, ranks: true, records: true, scoreThrough: 1799,
    why: 'DSS2 with Moby to the end (was Moby 1509, RNG 1626)' },
  // 2026-09-28 (later): 1211F8 also approaches +0x22C / +0x238 / +0x25C on the tick 12E9B8's jump release leaves a rail (Moby 5033),
  // so his re-attach at 5034 slides the same 0.21 cm: exact to the end.
  { name: 'parity-ai/ass1', args: ['--zoe', '--isolate'], humanThrough: 5300, ai: [5300], rngThrough: 5300, ranks: true, records: true, scoreThrough: 3301,
    why: 'ASS1 with Moby: his sliding crash bounces no longer dispatch 105D98 (was Moby 3673, RNG 3721, human 5103)' },
  { name: 'parity-ai/era5', args: ['--zoe', '--isolate'], humanThrough: 2799, ai: [2799, 2799, 2799, 2799, 2799], rngThrough: 2799, ranks: true, records: true, scoreThrough: 2799,
    why: 'Gravitude six riders to the end: the crash billboards (section LiveComp + slot-5 timer, builtins 28 / 54) break their ice pieces with the shared RNG (was RNG 1283, riders from 1472)' },
  { name: 'parity-ai/ara1-full', args: ['--zoe', '--isolate'], humanThrough: 3899, ai: [3899, 3899, 3899, 3899, 3899], rngThrough: 3899, ranks: true, records: true, scoreThrough: 3899,
    why: 'Snow Jam full race, six riders exact to the end (was human 3799, riders 3282/3587/3312/1644/2709, RNG 2404; Luther 3799 was the departure seed)' },
  // Peak 2 races as a six-rider world (the same captures as the peak2/*-race-ai physics gates, which place the human's draws
  // with their rng-order.json): the computer riders with their bank-2 stats (level 4 on Peak 2) and surface stats kept.
  // 2026-09-28: 1152 was Moby's 115D48 idle clock on a rail entered right after a soft collision: a stale soft-frame flag kept
  // 115D48 off. Human 1383 -> 2278, RNG 1151 -> 1695, Allegra / Luther 1683 -> 1693. Psymon (3443) and Moby (2136) now leave at
  // 2652 / 1869, after the shared RNG differs (1696): their old values held by chance.
  // 1694 (fixed the same day): Allegra lands onto Luther; the pair view lacked her touchdown's 106538 translation (committed after
  // the contacts in the port, before 107888 on the PS2). All six riders, the RNG and the ranks exact to the end.
  { name: 'peak2/cra3-race-ai', args: ['--zoe', '--isolate'], humanThrough: 5999, ai: [5999, 5999, 5999, 5999, 5999], rngThrough: 5999, ranks: true, records: true,
    why: 'Ruthless Ridge six riders to the end: jumps, tricks, soft collisions, rails, a landing onto another rider' },
  // 2026-09-28: the departure seed (13F2CC 1135B8 before the 13F358 clamp) moved the human 834 -> 5396 and the RNG 2438 -> 2639.
  // Riders 2 and 3 were exact to 3156 / 3082 only by chance: they ride on a shared RNG that already differs, and Allegra's trick
  // plan differs from 2985 on both cores. They now leave at 3142 / 2956, after the RNG (2640).
  // Later: a rider's own 121750 commits rider+0x9D0 to its cached world bones with its pair pushes (310530), so the next rail step
  // (13AF28) slides from the pushed board bone: riders 1 / 4 1856 -> 3927 / 4436, RNG 2640 -> 3773, riders 2 / 3 3142 / 2956 ->
  // 4533 / 4030. The human (isolated from the computer riders) now leaves at 5088, on a shared RNG that already differs (3773).
  { name: 'peak2/dra4-race-ai', args: ['--zoe', '--isolate'], humanThrough: 5087, ai: [3926, 4532, 4029, 4435, 4188], rngThrough: 3772, ranks: false, recordsThrough: 3929,
    why: 'Intimidator six riders: human and RNG much further since the departure seed fix; riders 2/3 after the RNG difference (luck)' },
  // Peak 2 (docs/peak2.md): Ruthless (DBC2) Rival Time vs Nate from the rolling start (bc-autopilot.mjs line, not isolated).
  // Nate needs his own stat getters (bank 2 raw 20, web/ai-racers.js set_rider_attributes). The blizzard (Weather payload 5:
  // 40 km/h at 90 deg) pushes the human from 2074 (the rider wind push 0x125970, docs/weather.md: 1 ulp at the first push);
  // 3911 the PS2 crashes out of the control-3 soft collision (control 8) where the browser stays in control 3 (open).
  // 2026-09-27: 3911 is the surface-18 hard crash of 13F178 (0x13F1C8 -> 10EB30 semantic 360; web/animation_bridge.cpp); was
  // 3910 / 6425 / 3932 / 3911. 4436 (fixed 2026-09-28): the crash reset came a tick late: after 136D40 detaches the board the rider
  // frame +0x160..+0x190 (1057B8's up, the +0x3F4 bounce counter) is 11FA10's board root with the identity local board = the physical
  // transform, not the free board (web/animation_bridge.cpp frameBoardRoot); human, Nate, RNG, ranks and pair records exact to 7000.
  // Nate (ai): provider words differ from the PS2 from 5004 in both builds; the surface-18 fix only exposes it earlier (6426 -> 5142,
  // while exact ticks went 6425 -> 6982/7000).
  { name: 'peak2/dbc2-race-tuck', args: ['--zoe', '--ticks', '7000'], humanThrough: 7000, ai: [7000], rngThrough: 7000, ranks: true, records: true,
    why: 'Ruthless Rival Time vs Nate, autopilot tuck line: rolling start, rival provider with Peak 2 stats, stage world, wind push, the surface-18 crash at 3911 and its reset (4436: the crash frame +0x160..+0x190 after the board detaches is the physical transform, 2026-09-28)' },
  { name: 'tunnel/dbc2-tunnel-ai', args: ['--zoe'], humanThrough: 11000, ai: [11000], rngThrough: 11000, ranks: true, records: true,
    why: 'Ruthless vs Nate, the dbc2 tuck line held to 11000 (watch: the human environment block 0x4FA370): through a tunnel volume at 9830 (stage builtin 74 sets rider+0x3FC, 2ED490 eases the selector; web/test-tunnel-lighting.mjs checks it bit for bit)' },
  // The same line re-captured with watches on the weather state (docs/weather.md; tools/export_weather.py objects of
  // ruthless-ready): the human's Weather painter (breath, wind) and the camera's (snowfall layers, splash) exact on every tick,
  // including Nate's reset placement at 1656 (0x2C03E8 resets every painter: web/shared_world.inc event 8). The camera eye
  // differs from the first camera update (rolling-start camera), so the splash speed / drops and the layer offsets are not gated.
  { name: 'weather/dbc2-weather', args: ['--zoe', '--shared-visual', '--visual-state', '../local/ps2-capture/runs/weather/dbc2-weather.visual-state.json', '--weather', '../local/ps2-capture/runs/weather/dbc2-weather.weather-map.json'],
    humanThrough: 3299, ai: [3299], rngThrough: 3299, ranks: true, recordsThrough: 3299, weatherExact: /^(rider\.|camera\.|layer\d\.count|splash\.snowfall)/,
    why: 'Ruthless blizzard: wind push, Weather painters, snowfall counts' },
  // The Throne (EBC3) Rival Time through its 14.5 km/h wind area (Weather payload 1, 300 deg; the rider painter >= 10 km/h on
  // 6475..7957): autopilot lines (local/ps2-capture/scripts/wx-ebc3-rail2.json: the first 4010 frames, a hard right at 4010 so
  // the rider takes a soft collision instead of the rail, then the autopilot, braking 300 ticks inside the area). The human
  // stays exact on all 7999 ticks through 324 wind pushes (6606..6929) and a reset (4136: fade painter resets 4155-4157); both
  // Weather painters, the layer counts and the splash counts exact on every tick; the camera eye / distance / splash speed until
  // the reset placement's camera (4157: the port rebuilds the chase camera, the PS2 does one set-target; open). Psymon leaves at
  // 2941 (his pair record 2946, the shared RNG 3053).
  { name: 'weather/ebc3-wind-rail2', args: ['--zoe', '--shared-visual', '--visual-state', '../local/ps2-capture/runs/weather/ebc3-wind-rail2.visual-state.json', '--weather', '../local/ps2-capture/runs/weather/ebc3-wind-rail2.weather-map.json'],
    // 2026-09-28: Psymon 2941 was 139A20's orientation tail skipped while he was airborne in control 7 after leaving a rail (2925).
    humanThrough: 7999, ai: [7999], rngThrough: 7999, ranks: true, records: true, weatherExact: /^(rider\.|camera\.cur|layer\d\.count|splash\.(snowfall|drops|crystals|pending))/,
    why: 'The Throne wind push (14.5 km/h), Weather painters' },
  // CTM heats (docs/ctm-events-in-world.md section 6), pending like the ctm-events human cases above. The riders come from the career lineup
  // document of the heat's countdown (tools/export_lineups.py export-career -> local/assets/native/ARA1/lineups-career/).
  { name: 'ctm-events/c0a-race', pending: 'eventInWorldAi', today: 'with --in-world-ai: everything END (the ctm-events/c0a-race-riders gate)', args: ['--zoe', '--ctm-countdown', '--document', '../local/assets/native/ARA1/lineups-career/ARA1-qual-zoe.json', '--in-world-ai', '--node-seed', '../local/ctm-events/caps/c0a-race.nodes.json'],
    humanThrough: END, ai: [END, END, END, END, END], rngThrough: END, why: 'CTM qualifier, six riders, event package: the human exact to the end (--ctm-countdown), the RNG leaves at 461, the computer riders later' },
  { name: 'ctm-events/c0c-race', pending: 'eventReturnInWorld', today: 'with --in-world-ai: everything END (the ctm-events/c0c-race-riders gate)', args: ['--zoe', '--ctm-countdown', '--document', '../local/assets/native/ARA1/lineups-career/ARA1-semi-zoe.json', '--in-world-ai', '--node-seed', '../local/ctm-events/caps/c0c-race.nodes.json'],
    humanThrough: END, ai: [END, END, END, END, END], rngThrough: END, why: 'CTM semi-final, six riders (WS13 roster, riders allocated out of actor-address order: ps2-capture-ai.mjs rosterOrder)' },
  // Stage 4 (pv eventInWorldAi): the computer riders set up as the page's in-world event makes them (compare-ai-capture.mjs --in-world-ai: the
  // event package into the first context in parts, the others by key, the countdown savestate's node states through the human into their
  // contexts) in a Conquer the Mountain race (0x535C11 = 0: the uncollected collectibles are listed and their slot-1 programs draw, 3 x
  // 0x341bbc at tick 460; the semi with the qualifier's collected bits): all six riders, the RNG, ranks and pair records exact to the end.
  // Captures: links to c0a-race / c0c-race (all 1400 / 1399 records).
  // The whole run from the Snow Jam arrival (free ride, the gate, WS1's hold, fly-over, approach and idle, the card, the race) with the six
  // riders in the streamed world (local/ctm-events/caps/c0a-full-ai: ps2_capture build --ai-state + capture_card.py --ai-dynamic;
  // compare-ai-capture.mjs --ctm-full, as the page runs pv eventInWorld + eventInWorldAi): the NIS director's camera point in the section
  // activation (core section_point), the riders fresh and held at their approach actors (ai-racers.js holdTick), npc_grid_start at the
  // countdown, 129768's list clear (section_restart). Human, riders, RNG, ranks and pair records exact to the end (1666 race ticks).
  { name: 'ctm-events/c0a-full-ai', coreExport: '_npc_grid_start', args: ['--zoe', '--in-world-ai', '--ctm-full', 'ARA1', '--document', '../local/assets/native/ARA1/lineups-career/ARA1-qual-zoe.json'],
    humanThrough: 1665, ai: [1665, 1665, 1665, 1665, 1665], rngThrough: 1665, ranks: true, records: true, why: 'CTM qualifier from the Snow Jam arrival, six riders, in the streamed world' },
  { name: 'ctm-events/c0a-race-riders', args: ['--zoe', '--ctm-countdown', '--document', '../local/assets/native/ARA1/lineups-career/ARA1-qual-zoe.json', '--in-world-ai', '--node-seed', '../local/ctm-events/caps/c0a-race.nodes.json'],
    humanThrough: 1399, ai: [1399, 1399, 1399, 1399, 1399], rngThrough: 1399, ranks: true, records: true, why: 'CTM qualifier, the in-world rider contexts (stage 4), the CTM collectibles (0x535C11 = 0)' },
  { name: 'ctm-events/c0c-race-riders', coreExport: '_stage_collection_list', args: ['--zoe', '--ctm-countdown', '--document', '../local/assets/native/ARA1/lineups-career/ARA1-semi-zoe.json', '--in-world-ai', '--node-seed', '../local/ctm-events/caps/c0c-race.nodes.json'],
    humanThrough: 1398, ai: [1398, 1398, 1398, 1398, 1398], rngThrough: 1398, ranks: true, records: true, why: 'CTM semi-final, the in-world rider contexts (stage 4), the qualifier\'s three collectibles collected' },
  // Stage 5 (pv eventReturnInWorld): the in-world return (local/ctm-events/caps/c0a-ret3: the qualifier, the pause's Give Up, the coast to the
  // auto replay's start, the results' Transport, the map's same-location confirm, all driven by ARMSX2's device pad through menu_pad.py).
  // compare-ai-capture.mjs --ctm-full runs the page's own return (web/event-return.js): the Transport's stop and WS14 frames with WS14's
  // enter, WS15 (free ride's settings, slot 1's player setup, every rider placed at Session point 1 through 11D390's free-ride branch),
  // the riders in pairs for 8 ticks (WS1's phases 1 / 2 without them), the removal with WS1's exit and its pose pass (129160), then the
  // human's free ride. --replay-return: through the results' auto replay (the countdown snapshot, 600 replayed ticks) and the Transport's
  // results-time snapshot (web/event-snapshot.js, docs/replay.md §2a). Gate from the return's record: all six exact until the removal, the human to the end, the RNG and ranks
  // everywhere, the pair records from the return. (The coast, race ticks 1700..1987, comes from the tick script, not the device pad: not gated here.)
  { name: 'ctm-events/c0a-ret3', coreExport: '_snapshot_save', returnGate: true, args: ['--zoe', '--in-world-ai', '--ctm-full', 'ARA1', '--replay-return', '--document', '../local/assets/native/ARA1/lineups-career/ARA1-qual-zoe.json'],
    humanThrough: 0, ai: [], rngThrough: Infinity, ranks: true, records: true, why: 'CTM qualifier, the in-world return to Session point 1' },
  // Carried presses (pv padCarry; docs/ctm-events-in-world.md "Carried presses"): c0a-ret2's pause menu Give Up, its Yes Cross (menu samples
  // 354..362) still held on the resumed ticks. The menu's samples go into the human's pad history (core pad_history_sample: cSSXApp_preUpdate's
  // 0x321298 on every app update), so tick 1699 reads Cross held with no press edge (word0 0x8000), and 1162C8's +0x360 latch (set: control 0
  // has run) requests no control 2: crouch target 1, brake 0 (0x1317B4), then the finish's control 10. Human, riders, RNG and ranks exact through
  // the coast (1987 race ticks) from the device pad.
  { name: 'ctm-events/c0a-ret2-coast', coreExport: '_pad_history_sample', args: ['--zoe', '--in-world-ai', '--ctm-full', 'ARA1', '--document', '../local/assets/native/ARA1/lineups-career/ARA1-qual-zoe.json', '--coast-only', '--coast-device', '--pad-carry'],
    humanThrough: 1987, ai: [1987, 1987, 1987, 1987, 1987], rngThrough: 1987, ranks: true, why: 'the Give Up\'s held Yes Cross carried into the coast (one pad history)' },
  // A player's full qualifier in the world (local/ctm-events/caps/c0a-ws13, local/ctm-events/ws13_capture.py: the Snow Jam arrival, the
  // gate, WS1, the card, the race ridden closed-loop by ps2_autopilot's Pilot to the human's own 3rd place, the results' Next heat, WS13,
  // the semi). Scored to race tick 11864 (--ticks 13171): Allegra's air crash at 5940 (12CA30 adds rider+0x9D0 with the instance push to
  // the posed board before 136D40 detaches it), her 115D48 reset at 9368 (131620: 115D48 before the 114CC0 reverse turn). At 11865 the
  // PS2 launches Snow Jam's rocket Spline (builtin 19's 0x35955C draw; pv peakSplines).
  { name: 'ctm-events/c0a-ws13', coreExport: '_npc_grid_start', args: ['--zoe', '--in-world-ai', '--ctm-full', 'ARA1', '--document', '../local/assets/native/ARA1/lineups-career/ARA1-qual-zoe.json', '--ticks', '13171'],
    humanThrough: 11864, ai: [11864, 11864, 11864, 11864, 11864], rngThrough: 11864, ranks: true, records: true, why: 'a player\'s whole CTM qualifier in the streamed world, six riders' },
  // The same qualifier with pv peakSplines (the streamed world's Spline pieces; on in PV_DEFAULTS): the EZseqTimer's EZrocketCore launches (11866 / 11872,
  // builtin 19's 0x359460 draw in the entity pass), Griff's dragontrig_1000 relaunch at 11944 (342E98's RestoreNode is listed and its
  // section leave restores the trigger; the released dragons' guard reaches every rider context, shared world event 10), and the human's
  // celebration (control 10's 115B58 play of 315 is a controller-phase draw: variant leaf 321), to the live stop (finish + 408, --ticks 13689).
  { name: 'ctm-events/c0a-ws13-splines', coreExport: '_npc_grid_start', args: ['--zoe', '--in-world-ai', '--ctm-full', 'ARA1', '--document', '../local/assets/native/ARA1/lineups-career/ARA1-qual-zoe.json', '--ticks', '13689'],
    humanThrough: 12382, ai: [12382, 12382, 12382, 12382, 12382], rngThrough: 12382, ranks: true, records: true, why: 'the whole CTM qualifier with the streamed Spline pieces (pv peakSplines)' },
  // WS13 through the semi (docs/ctm-events-in-world.md "WS13 through the semi"): the qualifier, Next heat's world reset 230180 (core
  // ctm_world_reset: 308C60's 308DB8 hides the Big Challenge markers again), the gondola, 1289F0's grid, the Continue and the semi to the
  // capture's end. Ticks restart at the Continue, so every baseline covers the whole run (exact everywhere).
  { name: 'ctm-events/c0a-ws13-semi', coreExport: '_event_row_enter', args: ['--zoe', '--in-world-ai', '--ctm-full', 'ARA1', '--ws13', '--document', '../local/assets/native/ARA1/lineups-career/ARA1-qual-zoe.json', '--ticks', '16100'],
    humanThrough: 16100, ai: [16100, 16100, 16100, 16100, 16100], rngThrough: 16100, ranks: true, records: true, why: 'a CTM qualifier, WS13 Next heat and the semi in the streamed world, six riders' },
  // Career race FINALS with the peak rival in slot 1 (docs/career-events.md "The peak rival in career events"; career-rival agent): Ruthless
  // Ridge and Intimidator, Zoe vs Nate (0x23A3D8: +0x44 = 0x145750), Psymon, Brodi (on Zoe), Griff, Elise, career race level 2. Derived PS2
  // finals local/reference/pcsx2/characters/career/{CRA3,DRA4}-final-zoe (peak2-arr -> Transport -> the qualifier -> Give Up -> results with
  // GMM+0x74 = 3 and the race handler +0xC = 0 -> Restart -> WS13 -> the Final Round card), captured from WS3 with --ai-state (tuck / weave
  // script p2-<course>-full, not isolated): local/ps2-capture/runs/careerrival -> local/career-rival/caps. The riders come from the countdown's
  // career document (tools/export_lineups.py export-career); web/test-career-rival.mjs shows the page's assembly (pv careerRival +
  // careerLevel) equals it leaf for leaf.
  { name: 'careerrival/cra3-final', args: ['--zoe', '--ctm-countdown', '--document', '../local/assets/native/CRA3/lineups-career/CRA3-final-zoe.json', '--in-world-ai', '--node-seed', '../local/career-rival/caps/cra3-final.nodes.json'],
    humanThrough: 1500, ai: [1500, 1500, 1500, 1500, 1500], rngThrough: 1500, ranks: true, records: true, scoreThrough: 1500, why: 'Ruthless Ridge career final, Nate in slot 1: human (score too), five riders, RNG, ranks, pair records exact to the end' },
  // 229: the human's hard crash (control 8, physics exact) is an ATTACKED bail on the PS2 (119B08 a1 != 0: score +0x12C, popup 0x2D); the
  // port's enter_crash (web/animation_bridge.cpp originalHardCrashEnter(..., false, ...)) always counts +0x124 (open, not career-specific).
  { name: 'careerrival/dra4-final', args: ['--zoe', '--ctm-countdown', '--document', '../local/assets/native/DRA4/lineups-career/DRA4-final-zoe.json', '--in-world-ai', '--node-seed', '../local/career-rival/caps/dra4-final.nodes.json'],
    humanThrough: 1500, ai: [1500, 1500, 1500, 1500, 1500], rngThrough: 1500, ranks: true, records: true, scoreThrough: 228, why: 'Intimidator career final, Nate in slot 1: human physics, five riders, RNG, ranks, pair records exact to the end; human score to 228' },
];
for (const c of aiCases) {
  if (only && !only.has(c.name)) continue;
  if (c.coreExport && !coreJsText.includes(c.coreExport)) { console.log(`skip ${c.name}: the core has no ${c.coreExport} export`); continue; }
  if (c.pending && !pendingRun(c)) { console.log(`pending ${c.name} (six riders) [pv ${c.pending}]: skipped (PENDING=1 scores it)`); continue; }
  if (c.pending) { const bin = runs + c.name + '.bin', report = reportPath(c.name, 'ai-regression.json');
    if (!fs.existsSync(bin)) { console.log(`skip ${c.name}: capture not present`); continue; }
    execFileSync(process.execPath, ['compare-ai-capture.mjs', bin, '--world-draws', '--report', report, ...c.args], { cwd: new URL('.', import.meta.url).pathname, stdio: ['ignore', 'ignore', 'inherit'] });
    const { summary, rows } = JSON.parse(fs.readFileSync(report, 'utf8')); const firstHuman = rows.find((r) => !r.humanExact)?.tick ?? Infinity;
    console.log(`pending [pv ${c.pending}] ${c.name} (six riders): human exact through ${firstHuman - 1}, computer riders ${summary.ai.map((a) => (a.firstInexact ? a.firstInexact.tick - 1 : 'all')).join('/')}, RNG through ${summary.firstRngMismatch ? summary.firstRngMismatch.tick - 1 : 'all'} (today: ${c.today})`);
    continue; }
  const bin = runs + c.name + '.bin';
  if (!fs.existsSync(bin) || !fs.existsSync(runs + c.name + '.capture.json')) { console.log(`skip ${c.name}: capture not present`); continue; }
  const report = reportPath(c.name, 'ai-regression.json');
  execFileSync(process.execPath, ['compare-ai-capture.mjs', bin, '--world-draws', '--report', report, ...c.args], { cwd: new URL('.', import.meta.url).pathname, stdio: ['ignore', 'ignore', 'inherit'] });
  const { summary, rows } = JSON.parse(fs.readFileSync(report, 'utf8'));
  if (c.returnGate) { const g = summary.ctmReturn; if (!g || g.row == null || g.outRow == null) throw new Error(`${c.name}: no in-world return in the report`);
    const human = rows.findIndex((r, k) => k >= g.row && !r.humanExact); if (human >= 0) throw new Error(`${c.name}: human left the original ${human - g.row} records after the return (tick ${rows[human].tick}; ${c.why})`);
    const ai = rows.findIndex((r, k) => k < g.outRow && r.ai.some((a) => !a.exact)); if (ai >= 0) throw new Error(`${c.name}: a computer rider left the original at row ${ai} (tick ${rows[ai].tick}; the return is row ${g.row})`);
    if (summary.firstRngMismatch) throw new Error(`${c.name}: shared RNG differs at ${summary.firstRngMismatch.tick}`);
    if (summary.firstRankMismatch) throw new Error(`${c.name}: race ranking +0xEC differs at ${summary.firstRankMismatch.tick}`);
    if (summary.firstReturnPairRecordMismatch) throw new Error(`${c.name}: 10F560 pair record differs after the return (tick ${summary.firstReturnPairRecordMismatch.tick})`);
    console.log(`${c.name}: all six exact from the return (row ${g.row}) to the removal (row ${g.outRow}), the human to the end (${rows.length - g.row} records), RNG / ranks everywhere, pair records from the return`); ran++; continue; }
  const firstHuman = rows.find((r) => !r.humanExact)?.tick ?? Infinity;
  if (firstHuman <= c.humanThrough) throw new Error(`${c.name}: human left the original at ${firstHuman} with real computer-rider RNG (baseline ${c.humanThrough}; ${c.why})`);
  c.ai.forEach((through, k) => { const bad = rows.find((r) => !r.ai[k].exact)?.tick ?? Infinity;
    if (bad <= through) throw new Error(`${c.name}: computer rider ${k + 1} (${summary.ai[k].character}) left the original at ${bad} (baseline ${through})`); });
  if (summary.firstRngMismatch && summary.firstRngMismatch.tick <= c.rngThrough) throw new Error(`${c.name}: shared RNG differs at ${summary.firstRngMismatch.tick} (baseline ${c.rngThrough})`);
  if (c.ranks && summary.firstRankMismatch) throw new Error(`${c.name}: race ranking +0xEC differs at ${summary.firstRankMismatch.tick}`);
  if (c.recordsThrough !== undefined && summary.firstPairRecordMismatch && summary.firstPairRecordMismatch.tick <= c.recordsThrough) throw new Error(`${c.name}: 10F560 pair record differs at ${summary.firstPairRecordMismatch.tick} (baseline ${c.recordsThrough})`);
  if (c.records && summary.firstPairRecordMismatch) throw new Error(`${c.name}: 10F560 pair record ${summary.firstPairRecordMismatch.slot}->${summary.firstPairRecordMismatch.other} differs at ${summary.firstPairRecordMismatch.tick}`);
  if (c.scoreThrough !== undefined) { const h = summary.humanScore; if (!h || !h.ticks) throw new Error(`${c.name}: no human score compare`);
    if (h.first && h.first.tick <= c.scoreThrough) throw new Error(`${c.name}: human score object left the original at ${h.first.tick} (${h.first.key}: web ${h.first.web}, PS2 ${h.first.ps2})`);
    if (h.firstBoost) throw new Error(`${c.name}: human boost words differ at ${h.firstBoost.tick}`); }
  if (c.rankThrough !== undefined && summary.firstRankMismatch && summary.firstRankMismatch.tick <= c.rankThrough) throw new Error(`${c.name}: race ranking +0xEC differs at ${summary.firstRankMismatch.tick} (baseline ${c.rankThrough})`);
  if (c.weatherExact) { const w = summary.weather; if (!w) throw new Error(`${c.name}: no weather compare (--weather)`);
    const keys = Object.keys(w.ticks).filter((k) => c.weatherExact.test(k)); if (!keys.length) throw new Error(`${c.name}: no weather fields gated`);
    for (const k of keys) if ((w.exact[k] || 0) !== w.ticks[k]) throw new Error(`${c.name}: weather ${k} left the original at ${w.first[k]?.tick} (web ${w.first[k]?.web}, PS2 ${w.first[k]?.ps2})`); }
  console.log(`${c.name}: human exact through ${firstHuman - 1}, computer riders ${summary.ai.map((a) => (a.firstInexact ? a.firstInexact.tick - 1 : 'all')).join('/')}, RNG through ${summary.firstRngMismatch ? summary.firstRngMismatch.tick - 1 : 'all'}`);
  ran++;
}
console.log(`PS2 capture regression: ${ran} scenario(s) checked.`);
if (!only && !process.env.CORE_JS && coreHash() === coreAtStart) { const d = new URL('node_modules/.cache/ssx-tests/', import.meta.url); fs.mkdirSync(d, { recursive: true }); fs.writeFileSync(new URL('captures-ok-core', d), coreAtStart + '\n'); }
