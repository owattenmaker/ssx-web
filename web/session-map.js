// The MCOMM Session map (OV.LUI 38session, overlay 0x20; pv sessionMap). PS2 code (SLUS_207.72):
//   0x2086A8 setup: the location's map picture MapPic = "|ses_<x>.ssh" (0x208F10 -> 0x1A37F8, name from table 0x440770 +4);
//   0x2087F0 the rows ("Top of run" / "Session point %d" / "Bottom of run"), the focus = 0x26B680's nearest point - 1;
//   0x209970 per point k (region bank 0x4D33A0 kind 2, index k: 26B5E0): 'Map Indicator Icon' sprite 'dot_visited' and
//            'Map Indicator Highlight' sprite 'indicator', 18 x 18, at the point's map pixel (layers 11 / 12);
//   0x2096A8 'Player Indicator Icon' sprite 'location', 17 x 17, at the rider's map pixel (layer 13);
//   0x209E78 on focus: the focused point shows its highlight, the others their dot.
// Table 0x440770 (stride 0x2C): course, picture, point count, the map square's corners (x0, y0, x1, y1) in world cm.
export const SESSION_MAP = Object.freeze({
  14: ['ses_ABC1', 7, [103430, -68737, -45458, 80152]], 17: ['ses_HUB_A', 1, [-106768, 63892, -61391, 18515]],
  0: ['ses_ARA1', 7, [-108660, -11855, -277488, 156973]], 5: ['ses_ASS1', 7, [-101896, 63353, -266619, 228079]],
  8: ['ses_ABA1', 2, [-30355, 60133, -96294, 126072]], 18: ['ses_HUB_B', 1, [-275270, 172085, -247800, 144616]],
  1: ['ses_BRA2', 8, [-313543, 146558, -52264, -114721]], 11: ['ses_BHP1', 2, [-300855, 134847, -251757, 85749]],
  19: ['ses_HUB_C', 1, [-192069, -8112, -154037, -46144]], 2: ['ses_CRA3', 7, [91271, -189660, -154476, 56087]],
  9: ['ses_CBA2', 2, [-75866, -39740, -137030, 21423]], 12: ['ses_CHP2', 2, [-219762, -70133, -160933, -128962]],
  20: ['ses_HUB_D', 1, [80204, -101191, 118686, -139673]], 3: ['ses_DRA4', 7, [126088, -110778, -37989, 53299]],
  6: ['ses_DSS2', 7, [162209, -261805, -8611, -90985]], 15: ['ses_DBC2', 6, [112469, -276947, -79560, -84918]],
  21: ['ses_HUB_E', 1, [-393400, 192368, -355783, 154750]], 4: ['ses_ERA5', 7, [-166332, -18570, -369392, 184489]],
  7: ['ses_ESS3', 6, [-173468, 156308, -343063, 325902]], 10: ['ses_EBA3', 2, [-403927, 155180, -472904, 224157]],
  13: ['ses_EHP3', 2, [-366835, 86593, -419779, 139538]], 16: ['ses_EBC3', 5, [-474009, 342568, -331204, 199763]],
});
// MapPic: group (250, 95) + (2, 2), 356 x 266 (0x43B2 356.0, 0x4385 266.0; the markers' origin 0xFC / 0x61).
export const MAP_RECT = Object.freeze([252, 97, 356, 266]);
export const DOT = 18, PLAYER = 17;   // 0x4190 18.0 (points), 0x4188 17.0 (player)
const f = Math.fround;
// 0x209A88 / 0x209754: |(p - c0) / (c1 - c0)| x size, cvt.w.s (toward zero), + origin, - 5.0: the marker's top-left, 640 x 480 LUI frame.
export function mapMarker(bounds, x, y) {
  const fx = Math.abs(f(f(f(x) - f(bounds[0])) / f(f(bounds[2]) - f(bounds[0]))));
  const fy = Math.abs(f(f(f(y) - f(bounds[1])) / f(f(bounds[3]) - f(bounds[1]))));
  return [Math.trunc(f(fx * MAP_RECT[2])) + MAP_RECT[0] - 5, Math.trunc(f(fy * MAP_RECT[3])) + MAP_RECT[1] - 5];
}
// 0x26B680: the nearest session point (kind 2, index 1..count) to the rider by 3D distance (strictly nearer wins, in index
// order); returns its index (1-based), 0 when there is none. points[k - 1] = [x, y, z] cm.
export function nearestSessionPoint(points, rider) {
  let best = Infinity, index = 0;
  points.forEach((p, k) => { if (!p) return; const dx = f(p[0] - rider[0]), dy = f(p[1] - rider[1]), dz = f(p[2] - rider[2]);
    const d = f(Math.sqrt(f(f(f(dx * dx) + f(dy * dy)) + f(dz * dz)))); if (d < best) { best = d; index = k + 1; } });
  return index;
}
