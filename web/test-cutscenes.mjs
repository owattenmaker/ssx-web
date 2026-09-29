// Cutscene (NIS) evaluation vs the live PS2 state (docs/cutscenes.md "Verification"): the Snow Jam single-event
// intro, script 89 race\ra_sgb_var1 (Manual camera, anchor 25 = start gate) at ticks 75 and 374, and script 73
// race\ra_sgb_static (Target camera) at tick 1, read from ARMSX2 RAM dumps (ScriptObjCam eye/target, fov).
// Also the selection (scfilter ScriptChoice), the trigger lists, anchors and clip timing. Needs the exported
// assets (tools/export_cutscenes.py); skipped with a note when they are missing.
import { readFileSync, existsSync } from 'node:fs';
import { curveAt, objectFrame, cameraAt, anchorFor, activeCut, clipSeconds, chooseScript, castWords, pickAlternatives,
  bindingRider, introSteps, podiumSteps, transportSteps, arrivalSteps, heatSteps, restartSteps, fadeAt, FLAG, scriptContainer, GROUP, toScene, actorRootAt } from './cutscenes.js';

const ROOT = new URL('./public/assets/CUTSCENES/', import.meta.url);
let failures = 0;
const check = (name, ok, detail = '') => { if (!ok) failures++; console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${detail ? ' ' + detail : ''}`); };
const near = (a, b, eps) => a.every((v, i) => Math.abs(v - b[i]) <= eps);

// Pure: the cubic keys.
check('curve cubic', Math.abs(curveAt([0, 0, 0, 1, 5, 10, 0, 0, 0, 7], 4) - 9) < 1e-12 && curveAt([0, 0, 0, 1, 5, 10, 0, 0, 0, 7], 12) === 7);
check('selection lists', introSteps('single').map((s) => s.group).join() === `${GROUP.GATE_VAR},${GROUP.GATE_IDLE}` &&
  introSteps('career').map((s) => s.group).join() === `${GROUP.FLYOVER},${GROUP.APPROACH},${GROUP.GATE_IDLE}` &&
  podiumSteps(0)[0].group === GROUP.PODIUM_WIN && podiumSteps(2)[0].group === GROUP.PODIUM_3RD &&
  transportSteps({ heli: true, departure: true }).map((s) => s.group).join() === `${GROUP.HELI_DEP},${GROUP.HELI_INAIR},${GROUP.HELI_INAIR_RIDER}` &&
  arrivalSteps('ABC1', true)[0].fmv === 29 &&
  heatSteps().map((s) => s.group).join() === `${GROUP.GOND_INAIR},${GROUP.GOND_INAIR_RIDER},${GROUP.GATE_IDLE}` && heatSteps()[1].holdTicks === 340);

if (!existsSync(new URL('index.json', ROOT))) {
  console.log('skip: web/public/assets/CUTSCENES missing (python3 tools/export_cutscenes.py)');
  process.exit(failures ? 1 : 0);
}
const json = (p) => JSON.parse(readFileSync(new URL(p, ROOT), 'utf8'));
const index = json('index.json'), locators = json('locators.json');
check('167 scripts', index.scripts.length === 167 && index.scripts[89].name === 'race\\ra_sgb_var1' && index.scripts[73].name === 'race\\ra_sgb_static');

// Anchors: locator 1 of ARA1 (mdl_ARA1_startgate_mainmodules_1000) = live (-131779.23, 13946.86, -228782.7), yaw 0.15010;
// anchor 25 = z - 1000 then ground-snapped: live (-131779.234, 13946.858, -228770.812).
const snap = () => -228770.812;
const a25 = anchorFor(25, { locations: locators.locations, location: 'ARA1', snap });
check('anchor 25 (start gate)', near(a25.pos, [-131779.234, 13946.858, -228770.812], 0.01) && Math.abs(a25.yaw - 0.15010) < 1e-4);
// Anchors 13..18 = grid nodes of race riders 0..5 (AIP kind-0 rows): live rider 5 (-131950.92, 14290.22, -228770.83), -170.49 deg.
const a18 = anchorFor(18, { locations: locators.locations, location: 'ARA1', subjectSlot: (s) => s - 15 });
check('anchor 18 (grid slot 5)', near(a18.pos, [-131950.92, 14290.22, -228770.83], 0.02) && Math.abs(a18.yaw * 180 / Math.PI + 170.49) < 0.01);

const script = (n, c) => json(`scripts/${c ?? scriptContainer(index.scripts[n], 'ARA1')}.${String(n).padStart(3, '0')}.json`);
const cam = (s) => s.tracks.flat().find((o) => o.kind <= 3);
{ // script 89 Manual camera at t = 75 / 374 (FORMAT verify_ram intro / intro2)
  const s = script(89), c = cam(s), f = objectFrame(a25, c.ext.offset);
  const p75 = cameraAt(c, f, 75), p374 = cameraAt(c, f, 374);
  check('89 eye t=75', near(p75.eye, [-131984.266, 14299.463, -228720.812], 0.03), JSON.stringify(p75.eye));
  check('89 target t=75', near(p75.target, [-131828.203, 14270.704, -228650.812], 0.03));
  check('89 eye t=374', near(p374.eye, [-131869.625, 13541.536, -228720.812], 0.03));
  check('89 target t=374', near(p374.target, [-131735.859, 13660.133, -228650.812], 0.03));
  check('89 fov 40 deg (live 0.6981317)', Math.abs(p75.fov - 0.6981317) < 1e-6);
  check('89 cut list camera 2 over 0..420', activeCut(s.tracks.flat().find((o) => o.kind === 4), 200).cam === 2 && s.duration === 420);
  check('89 fades 30 ticks in, 60 out', s.fade_in.type === 1 && s.fade_out.out_ticks === 60);
}
{ // script 73 Target camera at t = 1 (final.p2s)
  const s = script(73), c = cam(s), f = objectFrame(a25, c.ext.offset), p = cameraAt(c, f, 1);
  check('73 eye t=1', near(p.eye, [-132129.922, 14337.188, -228760.984], 0.03), JSON.stringify(p.eye));
  check('73 target t=1', near(p.target, [-131801.656, 14095.171, -228570.812], 0.03));
}
{ // actors of 89: 6 race riders bound to grid slots 0..5, clip i of anm00000089.afl; clip time 75/60 = 1.25 s (live)
  const s = script(89), actors = s.tracks.flat().filter((o) => o.kind === 5);
  check('89 actors bind race riders 0..5 (anchors 13..18)', actors.map((a) => `${a.ext.binding}@${a.ext.anchor}`).join() === '16@13,17@14,18@15,19@16,20@17,21@18');
  check('89 clip i, time 1.25 s at t=75', actors.every((a, i) => a.ch[0].i[0].v === i) && Math.abs(clipSeconds(actors[0].ch[0].i[0], 75) - 1.25) < 1e-9);
  const lib = json('anim/00000089.json');
  check('89 clips decoded (6, 211 frames for rider 0)', lib.clips.length >= 6 && lib.clips[0].frame_count === 211 && !!lib.clips[0].streams['0']);
  const race = [0, 1, 2, 3, 4, 5].map((i) => ({ id: 'r' + i, character: i, slot: i }));
  const cast = { humans: [race[0]], race, ai: race.slice(1), roles: [] };
  check('binding 16 -> race rider 0 = human', bindingRider(cast, 16) === race[0] && bindingRider(cast, 4) === race[0]);
  const picked = pickAlternatives(s, cast, () => 0);
  check('one alternative per track', picked.length === s.tracks.length);
  const f = objectFrame(a18, actors[5].ext.offset), r = actorRootAt(actors[5], f, 0);
  check('actor 5 root on its grid node', near(r.pos, a18.pos, 1e3));
}
{ // ScriptChoice on scfilterARA1: list 4 = var1/var2, list 9 = the winner's three podium scenes, list 25 = rival
  const f = index.filters.ARA1;
  const zoe = { character: 4 }, mac = { character: 3 };
  const words = castWords({ roles: [zoe], humans: [zoe], ai: [], race: [zoe] });
  const podium = new Set(); for (let i = 0; i < 60; i++) podium.add(chooseScript(f[GROUP.PODIUM_WIN], words, {}, Math.random));
  check('podium win: Zoe a/b/c (60/61/62)', [...podium].sort().join() === '60,61,62');
  const macw = castWords({ roles: [mac], humans: [mac], ai: [], race: [mac] });
  check('podium win: Mac (49/50/30)', new Set([0, 0.5, 0.99].map((r) => chooseScript(f[GROUP.PODIUM_WIN], macw, {}, () => r))).size === 3);
  check('gate var: 89 or 96', [89, 96].includes(chooseScript(f[GROUP.GATE_VAR], words)));
  check('least played first', chooseScript(f[GROUP.GATE_VAR], words, { 89: 1 }) === 96);
  check('rival race scene of Zoe = ra_bc_zoe (10)', chooseScript(f[GROUP.RIVAL_RIDER], words) === 10);
}
{ // container choice: 73 at ARA1 from scdat_ARA1, 89 standalone, 149 resident scdat_main
  check('containers', scriptContainer(index.scripts[73], 'ARA1') === 'scdat_ARA1' && scriptContainer(index.scripts[89], 'ARA1') === '00000089' &&
    scriptContainer(index.scripts[149], 'ARA1') === 'scdat_main');
}
if (existsSync(new URL('PROPS/pda/prop.json', ROOT))) {   // the station handheld: rigid on handright, one flip-open morph
  const pda = json('PROPS/pda/prop.json'), lib = json('anim/scdat_A.json');
  check('PDA prop on handright, flip morph, lodge clip shows it at frame 40', pda.bone.index === 15 && pda.morphs.length === 1 &&
    JSON.stringify(lib.clips[0].events) === '[[40,0],[0,1]]' && lib.clips[0].streams['11'].channels === 1);
}
// Conquer the Mountain flow (docs/ctm-flow.md): the ride-in intro starts at the approach; the final heat queues the start hut;
// fades chain through the previous step's fade-out record (PS2 frame means: ps2b/intro s1040 19 -> s1045 83 (approach bright
// at t3), metro-intro s483 bright at t4, the idle under the card fades in over ~30 (approach fade-out in 30), semi-intro
// #146 fades in over ~30 (gond_inair fade-out in 30), not its own 60).
{
  check('ride-in / final heat / restart lists', introSteps('career-ridein').map((s) => s.group).join() === `${GROUP.APPROACH},${GROUP.GATE_IDLE}` &&
    introSteps('career')[0].loading === true && heatSteps(true).map((s) => s.group).join() === `${GROUP.GOND_INAIR},${GROUP.GOND_INAIR_RIDER},${GROUP.GATE_VAR},${GROUP.GATE_IDLE}` &&
    restartSteps()[0].idle === true);
  const fly = json('scripts/00000094.094.json'), app = json('scripts/00000066.066.json'), idle = json('scripts/scdat_ARA1.073.json');
  const gond = json('scripts/scdat_main.150.json'), gondZoe = json('scripts/00000146.146.json');
  const a = (o) => +o.alpha.toFixed(3);
  check('fly-over fades in over 30 and out over its last 30', a(fadeAt({ script: fly, t: 0, duration: 270 })) === 1 && a(fadeAt({ script: fly, t: 15, duration: 270 })) === 0.5 &&
    a(fadeAt({ script: fly, t: 100, duration: 270 })) === 0 && a(fadeAt({ script: fly, t: 255, duration: 270 })) === 0.5);
  check('approach after the fly-over is bright at t3 (own fade-in unused)', a(fadeAt({ script: app, t: 3, duration: 242, prevFadeOut: fly.fade_out })) === 0 &&
    a(fadeAt({ script: app, t: 3, duration: 242 })) === 0.9);
  check('idle under the card fades in with the approach fade-out in 30', a(fadeAt({ script: idle, t: 15, duration: 60, prevFadeOut: app.fade_out, step: { idle: true } })) === 0.5);
  check('gond_inair_zoe fades in over gond_inair fade-out in 30', a(fadeAt({ script: gondZoe, t: 15, duration: 560, prevFadeOut: gond.fade_out, step: { flags: FLAG.HOLD, holdTicks: 340 } })) === 0.5 &&
    a(fadeAt({ script: gondZoe, t: 330, duration: 560, prevFadeOut: gond.fade_out, step: { flags: FLAG.HOLD, holdTicks: 340 }, held: 325 })) === 0.5);
  // kind-7 audio of the flow (ctm/audio NIS-AUDIO.md): fly-over music code 21 at t0, PA cue 15 at t30, its own sound 0
  // (t 0..194, CHARACTER); approach PA cue 2 (PA_Rider_Race_Intro); podium PA cue 7 (PA_Medals).
  const ctl = (sc) => sc.tracks.map((t) => t[0]).find((o) => o.kind === 7);
  const c94 = ctl(fly), c66 = ctl(app), c60 = ctl(json('scripts/00000060.060.json'));
  check('flow cues', c94.ch[1].i[0].w[0] === 21 && c94.ch[2].i[0].w[0] === 15 && c94.ch[2].i[0].t === 30 && c94.ch[3].i[0].v === 0 && c94.ch[3].i[0].t1 === 194 &&
    c66.ch[2].i[0].w[0] === 2 && c60.ch[2].i[0].w[0] === 7);
}
check('toScene (x, z, -y) / 100', near(toScene([100, 200, 300], { x: 1, y: 2, z: 3 }), [0, 1, -5], 1e-12));
// The rider held by a cut's human actor (binding 4) at the step's first tick (123640: M(+0x700) . key 0; main.js nisHoldAt / onHumanActor):
// lodge_arr3 at Green (anchor 19 ground-snapped, key 0 z -15: PS2 ctm-parity/door-no pre.p2s z -214274.3 under the locator's -214259.3),
// heli_inair #149 / heli_inair_zoe #122 on the TRANSP heli locator (anchor 29 of a looping script; PS2 ctm-parity/transport to-c records
// 924 / 1044: (-306117.1, -99303.9, -648440.6) q (0, 0, -1, 0), (-306102.1, -99403.9, -648279.6) q (0, 0, -0.7071, 0.7071)).
{
  const human = (file, location, loop, snapZ = null) => { const sc = json(`scripts/${file}`), o = sc.tracks.map((t) => t[0]).find((x) => x?.kind === 5 && x.ext.binding === 4);
    const a = anchorFor(o.ext.anchor, { locations: locators.locations, location, loop, snap: snapZ == null ? undefined : () => snapZ }); return actorRootAt(o, objectFrame(a, o.ext.offset), 0); };
  const lodge = human('scdat_A.148.json', 'A', false, -214259.3);
  check('lodge_arr3 rider 15 cm under the locator', near(lodge.pos, [-72133.0, 37460.2, -214274.3], 0.1));
  const inair = human('scdat_main.149.json', 'DBC2', true), loop = human('00000122.122.json', 'DBC2', true);
  check('heli_inair / heli_inair_zoe rider on the TRANSP heli', near(inair.pos, [-306117.1, -99303.9, -648440.6], 0.1) && near(loop.pos, [-306102.1, -99403.9, -648279.6], 0.1) &&
    near(inair.quat, [0, 0, -1, 0], 1e-4) && near(loop.quat, [0, 0, -Math.SQRT1_2, Math.SQRT1_2], 1e-4));
}
if (failures) { console.error(`${failures} failure(s)`); process.exit(1); }
console.log('cutscenes ok');
