// Stage teleports an honest online racer makes (web/server/plausibility.mjs): the Metro-City phone booths and water tower,
// stage builtin 34 -> rider vt+0x54 0x123210 (web/stage_teleport.inc, docs/stage-teleport.md). A contact with the trigger
// runs the stage program, which draws the gameplay RNG and places the rider on the probed ground at one destination:
//   P = r3 + 100 (a r0 + b r1), (a, b) by the rider's roster slot (the human (2, 0)), heading r0, speed kept;
//   the ground probe runs from P.z + 200 down to P.z - 7000 (11D660, clearance 0).
// The teleport bumps the packet's reset placements by one (it places through 11D660 -> reset_place_at), but the check
// does not rely on it (web/net/rider-packet.js placements).
//
// TELEPORT_BEAMS is built from the stage data by buildTeleportBeams() (web/test-mp-plausibility.mjs compares the two):
//   trigger box   the trigger instance's world bounds (<LOC>/world_collision.json instances, the type-2 AABB node);
//   destinations  instance+0x10 of each destination (<LOC>/STAGE/stage-world.json teleports, matrix_bits);
//   which trigger goes where: the stage programs 417 (tower: 90 % 0xA2C10, else 0x22410), 419 (booth 0004: 75 % 0x99010,
//   else 0x68F10) and 420 (booth 0007: 0x24210) of BRA2 track 16 (docs/stage-teleport.md section 2).
// The same resources and world coordinates are part of the streamed worlds (PEAK1, MOUNTAIN*).
export const BEAM_PROGRAMS = Object.freeze({ BRA2: [[0x43510, [0xA2C10, 0x22410]], [0xC9810, [0x99010, 0x68F10]], [0x55B10, [0x24210]]] });
export const SLOT_OFFSETS = Object.freeze([[2, 0], [1, 0], [2, -2], [2, 2], [1, -2], [2, 1]]); // rider+0x86C 0..5 (0x123210)
export const PROBE_UP = 200, PROBE_DOWN = 7000;   // cm around P.z (11D660 clearance 0)

export const TELEPORT_BEAMS = Object.freeze({
  BRA2: [
    { trigger: 0x43510, name: 'mdl_BRA2_watertowerbeam_0001', min: [-198076.125, 35412.28125, -515694.5625], max: [-197513.828125, 36003.5234375, -514639.6875],
      destinations: [
        { resource: 0xA2C10, name: 'mdl_BRA2_watertowerbeam_0002', r0: [0.49999842047691345, -0.8660263419151306, 0], r1: [0.8660263419151306, 0.49999842047691345, 0], r3: [-189369.34375, 20046.28515625, -525386.75] },
        { resource: 0x22410, name: 'mdl_BRA2_watertowerbeam_0003', r0: [0.49999842047691345, -0.8660263419151306, 0], r1: [0.8660263419151306, 0.49999842047691345, 0], r3: [-176292.765625, 12852.0283203125, -536744.1875] }] },
    { trigger: 0xC9810, name: 'mdl_BRA2_phoneboothbeam_0004', min: [-206774.625, 78158.1796875, -499044.5625], max: [-206676.578125, 78212.3671875, -498827.59375],
      destinations: [
        { resource: 0x99010, name: 'mdl_BRA2_phoneboothbeam_0005', r0: [0.006644871551543474, -0.9999779462814331, 0], r1: [0.9999779462814331, 0.006644871551543474, 0], r3: [-200533.609375, 65509.171875, -502328.5625] },
        { resource: 0x68F10, name: 'mdl_BRA2_phoneboothbeam_0006', r0: [-0.007540325168520212, -0.9999409914016724, -0.007819348946213722], r1: [0.9997002482414246, -0.007355902809649706, -0.02335183322429657], r3: [-213424.265625, 76222.0078125, -498480.625] }] },
    { trigger: 0x55B10, name: 'mdl_BRA2_phoneboothbeam_0007', min: [-171142.71875, -48903.1796875, -590252.875], max: [-171048.9375, -48844.55078125, -590035.9375],
      destinations: [
        { resource: 0x24210, name: 'mdl_BRA2_phoneboothbeam_0008', r0: [-0.690794825553894, -0.7230508327484131, 0], r1: [0.7230508327484131, -0.690794825553894, 0], r3: [-161153.15625, -78283.296875, -620595.125] }] },
  ],
});

// The beams of an online race's course: Metro-City itself and the streamed worlds that hold it.
export function beamsFor(course) {
  const c = String(course ?? '').toUpperCase();
  return c === 'BRA2' || c === 'PEAK1' || c.startsWith('MOUNTAIN') ? TELEPORT_BEAMS.BRA2 : [];
}

// Build the table from the stage data (readJson(path relative to web/public/assets/)).
export function buildTeleportBeams(readJson, location = 'BRA2') {
  const stage = readJson(`${location}/STAGE/stage-world.json`), collision = readJson(`${location}/world_collision.json`);
  const f = (u) => new Float32Array(new Uint32Array([u]).buffer)[0];
  return BEAM_PROGRAMS[location].map(([trigger, destinations]) => {
    const i = collision.instances.find((x) => x.track === (trigger & 0xff) && x.rid === trigger >>> 8);
    return { trigger, name: i.name, min: i.bounds_min_cm, max: i.bounds_max_cm,
      destinations: destinations.map((resource) => { const t = stage.teleports.find((x) => x.resource === resource), m = t.matrix_bits.map(f);
        return { resource, name: t.name, r0: m.slice(0, 3), r1: m.slice(4, 7), r3: m.slice(12, 15) }; }) };
  });
}

// Distance (cm) from a point to a beam's trigger box.
const boxDistance = (p, { min, max }) => Math.hypot(...[0, 1, 2].map((k) => Math.max(min[k] - p[k], 0, p[k] - max[k])));
// Distance (cm) from a point to where a destination can place the rider (any roster slot; the probed ground below P).
function landingDistance(p, d) {
  let best = Infinity;
  for (const [a, b] of SLOT_OFFSETS) {
    const P = [0, 1, 2].map((k) => d.r3[k] + 100 * (a * d.r0[k] + b * d.r1[k]));
    const dz = Math.max(P[2] - PROBE_DOWN - p[2], 0, p[2] - (P[2] + PROBE_UP));
    best = Math.min(best, Math.hypot(p[0] - P[0], p[1] - P[1], dz));
  }
  return best;
}
// A jump from `from` to `to` that one of the beams explains: the rider reached the trigger and left the destination within
// `reach` cm of travel in total (the packet interval at the speed bound, plus the body's reach into the trigger).
// Returns {trigger, destination} or null.
export function teleportJump(beams, from, to, reach) {
  for (const beam of beams) {
    const before = boxDistance(from, beam); if (before > reach) continue;
    for (const d of beam.destinations) if (before + landingDistance(to, d) <= reach) return { trigger: beam.trigger, destination: d.resource };
  }
  return null;
}
