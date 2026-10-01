// The PS2 render list's draw order (docs/visual-parity.md 41.9), shared by the world pass and the post-fog effects.
//
// Every draw of a frame becomes a render record (128 bytes, render list = renderer +6384). The flush 0x363490 merges records of equal
// material state and textures into buckets (0x362DE8, hashes 0x394ED0 / 0x395000), keys each bucket with 0x364240 and radix-sorts the
// (key, bucket) pairs ascending and stable (0x364050, pairs at list +0x67CA8), then draws them in that order:
//   key = ~((31 - priority) << 26 | t0 << 16 | rank(word0 bits 6..9) << 13 | (texture & 0x3FF) << 3)
//   - priority = word2 bits 5..9 (layer 0 is priorities 0..5, the world; 6..8 run after the fog composite 36AC00, the encoded pass);
//   - t0 from word1 bits 0..1: 0 -> 1023, 1 -> 1022, 2 -> word2 bits 10..28 (the depth key of the sorted classes), 3 -> 0;
//   - rank = the jump table at 0x492010 over word0 bits 6..9: {0: 0, 1: 4, 2: 5, 3: 3, 4: 1, 5: 6, 6: 2, 7: 2, 8: 1}, else 0;
//   - texture = the first texture handle (when >= 0) | (second handle & 0xF) << 6.
// The key is inverted, so at one priority the draws run by descending t0, then descending rank, then descending texture bits;
// equal keys keep their bucket's first submission. The effects' fields, from the code and the last frame's sorted list in PS2 RAM
// (351 kept states, tools: docs/visual-parity.md 41.9):
//   - the render methods set word0 bits 6..9: particle sprites 380CE0 / 380518 (snow, set-piece Particle / DynamicParticle, spark
//     kernel, grind chunks, terrain sparkle) 4 -> rank 1; strips (wake, boost ribbons, aura, beam, '!' icon) 3 -> rank 3;
//     snowfall 381310 / 3816F0 5 -> rank 6; the 377CF0 / 3781A0 sprites keep the base material's 0 -> rank 0 (light glows,
//     halos, camera splash, fist sparkle 2F1510 from material 0x501420);
//   - t0: word1 bits 0..1 = 2 with word2 bits 10.. clear -> 0 for the effects; the fog puffs (0x2DBF98) and the priority-8 halos,
//     streamers, lens / sun and splash sprites keep word1 bits 0..1 = 0 -> 1023;
//   - textures: the FX table at renderer +0xF50 (index = the FX id of the tag table 0x4891B0). Its handles are the texture
//     manager's, so they differ between an event boot and Conquer the Mountain (two tables, below);
//   - the second texture: effects that push the current material inherit its second slot. The world pass sets it to FX 46
//     'spec' (renderer +36 = 46 in every kept state; 0x22B374, 0x22C0BC), and the set-piece particles (3708C0 pushes the top,
//     371380 copies the emitter's own material) carry it in most frames (ABC1: every snowwind bucket is brth + spec); the rider
//     FX (snow, breath, wake, boost) never do.
export const FX_HANDLES = Object.freeze({
  // renderer +0xF50 in PS2 RAM: setpieces-abc1/full tick 2000 (every event state), allpeak/apr-full tick 10507 (every CTM state)
  event: Object.freeze([
    1525, -1, -1, -1, 1524, 1523, 1522, 1521, 1520, -1, 1519, 1518, -1, 1517, 1516, 1515, 1514, 1513, 1512, 1511, 1510, 1509, 1508, 1507,
    1506, 1505, 1565, 1564, 1563, 1562, 1561, 1560, 1559, -1, -1, -1, -1, 1558, 1557, 1556, 1555, 1554, 1553, 1552, 1551, 1550, 1549, -1,
    -1, -1, -1, 1548, 1547, 1546, 1545, 1544, 1653, 1652, 1651, 1650, 1649, 1648, 1647, 1646, 1645, 1644, 1643, 1642, 1641, -1, 1640, 1639,
    1638, 1637, 1636, -1, -1, -1, -1, -1
  ]),
  ctm: Object.freeze([
    1531, -1, -1, -1, 1530, 1529, 1528, 1527, 1526, -1, 1525, 1524, -1, 1523, 1522, 1521, 1520, 1519, 1518, 1517, 1516, 1515, 1514, 1513,
    1512, 1511, 1510, 1509, 1508, 1507, 1506, 1505, 1565, -1, -1, -1, -1, 1564, 1563, 1562, 1561, 1560, 1559, 1558, 1557, 1556, 1555, -1,
    -1, -1, -1, 1554, 1553, 1552, 1551, 1550, 1549, 1548, 1547, 1546, 1545, 1544, 1648, 1647, 1646, 1645, 1644, 1643, 1642, -1, 1641, 1640,
    1639, 1638, 1637, -1, -1, -1, -1, -1
  ])
});
export const FX_SPEC = 46; // 'spec': renderer +36, the second texture the world pass leaves in the current material
export const MODE_RANK = Object.freeze([0, 4, 5, 3, 1, 6, 2, 2, 1]); // 0x492010 over word0 bits 6..9
let handles = FX_HANDLES.event;
// main.js loadCourse: a Conquer the Mountain world (course.freeRide) uses the CTM boot's texture handles.
export function setDrawOrderWorld(ctm) { handles = ctm ? FX_HANDLES.ctm : FX_HANDLES.event; }
export const fxHandle = (id) => (id >= 0 && id < handles.length ? handles[id] : -1);

// 0x364240 for {priority, t0, mode (word0 bits 6..9), fx / fx2 (FX ids) or handle / handle2}: the unsigned 32-bit key.
export function drawKey({ priority = 7, t0 = 0, mode = 0, fx = -1, fx2 = -1, handle = fxHandle(fx), handle2 = fxHandle(fx2) }) {
  let tex = handle >= 0 ? handle : 0;
  if (handle2 >= 0) tex |= (handle2 & 0xF) << 6;
  const x = ((31 - priority) << 26 | (t0 & 0x3FF) << 16 | (MODE_RANK[mode] ?? 0) << 13 | (tex & 0x3FF) << 3) >>> 0;
  return ~x >>> 0;
}
// three.js renderOrder for a post-fog effect: priorities 6 / 7 / 8 in [660, 680) / [680, 700) / [700, 720), ascending with the key
// (adjacent textures differ by 3.6e-6); `submission` (0..99) orders equal keys like the PS2's first-submitted bucket.
export const EFFECT_ORDER_BASE = 660;
export function drawOrder(spec, submission = 0) {
  return EFFECT_ORDER_BASE + (drawKey(spec) - 0x98000000) / 0x0C000000 * 60 + submission * 1e-8;
}
// The world pass (sortedClass, docs/visual-parity.md 44): at one priority the opaque states (t0 1023) and the class-1 models
// (1022) draw before the depth-sorted classes 2 / 3 (their word2 depth key, back to front: three's transparent sort). The world's
// own draws stay at 0; the sorted classes at 0.5; the effects (drawOrder) after them.
export const WORLD_SORTED_ORDER = 0.5;

// The effects' fields (docs/visual-parity.md 41.9) and their submission order in the frame: the world pass (entity draws, fog
// puffs 22C708), then each rider's FX (0x1119F8: wake, sparks, boost, aura, streamers, '!' icon, beam, snow, fist).
export const EFFECT = Object.freeze({
  fogPuffs: { priority: 7, t0: 1023, mode: 3, fx: 4 },          // 0x2DBF98: fog0
  setPieceParticle: (fx) => ({ priority: 7, mode: 4, fx, fx2: FX_SPEC }), // 345AD0 / 345F90 -> 3708C0 / 371380
  wake: { priority: 7, mode: 3, fx: 56 },                       // 0x2DDAB8 -> 3883B8
  chunks: (fx) => ({ priority: 7, mode: 4, fx }),               // 0x2DB478 -> 371688 (tmb1..8)
  sparks: { priority: 7, mode: 4, fx: 22 },                     // 0x2DB478 -> 380518 (sprk)
  glints: { priority: 7, mode: 0, fx: 22 },                     // 0x2DB478 -> 377CF0 (sprk)
  boost: (fx) => ({ priority: 7, mode: 3, fx }),                // 0x2E7A10: yrbn .. prbn (57..61)
  aura: { priority: 7, mode: 3, fx: 62 },                       // 0x2EB198: psmr
  streamer: (fx) => ({ priority: 8, t0: 1023, mode: 3, fx }),   // 0x2EF950: strm 63 / prbn 61
  icon: { priority: 7, mode: 3, fx: 23 },                       // 0x2D5048: exlm
  beam: { priority: 7, mode: 3, fx: 43 },                       // 0x2E3AF8: beam
  snow: (fx) => ({ priority: 7, mode: 4, fx }),                 // 0x2E24D0 -> 371688 -> 380CE0
  fist: { priority: 7, mode: 0, fx: 24 },                       // 0x2F1510 -> 377CF0 (ospk)
  snowfall: { priority: 7, mode: 5, fx: 8 },                    // 0x2E6008 -> 381310 / 3816F0 (sfal)
  splash: (fx) => ({ priority: 8, t0: 1023, mode: 0, fx }),     // 0x2F2C30 / 0x2F3418: ices 66 / icel 67
  halo: (fx) => ({ priority: 8, t0: 1023, mode: 0, fx }),       // 0x2D1D10 -> 377CF0 / 3781A0: blha .. whha (37..42)
});
export const SUBMIT = Object.freeze({ setPiece: 1, fogPuffs: 2, halo: 3, avalanche: 5, wake: 10, impact: 11, boost: 13, aura: 14, streamer: 15, icon: 16, beam: 17, snow: 18, fist: 19, weather: 30 });
