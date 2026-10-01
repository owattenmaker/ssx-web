// The rider's controller light list: the trick boost's extra rider light (docs/presentation.md "Trick-boost light").
// Rider manager 0x128AC0, every tick and rider: 1218D0 clears the control owner's list (rider+0x77C -> +0xD30, 392D18:
// ambient 0, count 0), then the power-up aura update 2EADD0 (after the boost ribbon 2E66B8) adds one directional light
// while the trick boost holds (rider+0x2EC > 0, or the gp+0x1630 switch): 392D90(list, direction (0, 0, 1, 0), colour
// (2, 2, 2)); colour at +0x10 + 12i, direction at +0x50 + 16i, count at +0xC. 1220D8 adds the list to the rider's
// irradiance (389558 ambient, 389520 / 389308 directional): the numeric path shade_rider_lighting's `extra` takes
// (engine/rider_irradiance.hpp; tools/test_rider_irradiance_native.py writes this D30 layout into the original 1220D8
// and matches all 40 words), so the colour is in the environment bank's own units (x 255 for the GPU coefficients),
// the direction in the bank's z-up frame, and the values go in unchanged. Every rider holding a trick boost gets it,
// the computer riders too (opponent-riders.js). The core keeps the list (web/boost_gameplay.inc rider_controller_lights:
// [count, ambient RGB, direction XYZ, colour RGB]); an older core without it falls back to boost_info()[4] here.
export const TRICK_BOOST_LIGHT = Object.freeze({ ambient: [0, 0, 0], direction: [0, 0, 1], colour: [2, 2, 2] });
export const trickBoostActive = (boostInfo) => boostInfo[4] > 0;   // core boost_info()[4] = boostState.modifier (rider+0x2EC)

// The extra-light block from boost_info() (fallback), written into `buffer` (Float32Array >= 9): ambient RGB, then
// direction XYZ + colour RGB per light. Returns the count.
export function controllerLights(boostInfo, buffer) {
  if (!trickBoostActive(boostInfo)) return 0;
  const L = TRICK_BOOST_LIGHT;
  buffer.set(L.ambient, 0); buffer.set(L.direction, 3); buffer.set(L.colour, 6);
  return 1;
}

// Per core (or rider context view): count() -> this tick's light count; `pointer` is the extra block to pass with it
// (core._shade_rider_lighting(env, view, point, rim, constants, count ? pointer : 0, count)). No allocation per frame.
export function createControllerLights(core) {
  const off = /[?&]boostlight=0\b/.test(globalThis.location?.search ?? '');
  if (core._rider_controller_lights) {
    const list = core._rider_controller_lights();   // fixed per rider context (RIDER_LOCAL static)
    return { pointer: list + 4, count() { return off ? 0 : Math.round(core.HEAPF32[list >> 2]); }, dispose() {} };
  }
  const pointer = core._malloc(36);
  let info = null, block = null, at = 0;
  return {
    pointer,
    count() {
      if (off || !core._boost_info) return 0;
      const p = core._boost_info();   // fills the rider context's boost_info() block (core.cpp)
      if (!info || p !== at || info.buffer !== core.HEAPF32.buffer) { at = p; info = new Float32Array(core.HEAPF32.buffer, p, 8); block = new Float32Array(core.HEAPF32.buffer, pointer, 9); }
      return controllerLights(info, block);
    },
    dispose() { core._free(pointer); },
  };
}
