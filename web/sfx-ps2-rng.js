// pv sfxPs2Rng (docs/audio-logic.md 5.6): the audio's random picks on the PS2 presentation generator 0x4FF018.
//   2ADF60 (rand15) = BXrand 0x3177F0 & 0x7FFF; BXrand = 0x317A08 on the six words at 0x4FF018 (lui 0x50, addiu -0xFE8).
//   It is the stream the world effects draw from (snow / grind emitters, splash, lightning, crowd flashes, flags, camera
//   shake; docs/visual-rng-order.md), not the game RNG 0x4FF030: the simulation does not read it.
//   The page's stream is the human core's visual_rng_words (the computer riders' cores borrow it, web/ai-racers.js).
// Rules (SLUS_207.72):
//   296310 crash sound: 0x30 + (r & 0x7FFF) * 3 / 0x7FFF (divu at 0x29646C); no draw on surface 2 / 3 (LAND sound 9).
//   29F660 grunt: kind 1 -> (r & 0x7FFF) * 3 / 0x7FFF, kind 0 -> the same + 3, kind 2 -> the counter audio+0x5734 (no draw);
//     drawn only for the human's bank 0xE (29F620), after the gates 29F0A0, 288AE0, +0x470 and the 295028 repeat check.
//   2A4CA0 crowd pick: list[BXrand % n], no draw when n = 0 (2A64A0 trick, 2A6648 fall, 2A5AE8 anticipation).
//   ARMSX2 (local/sfx-rng/zoe-rng): all 20 audio draws in 5205 ticks give the variant the PS2 then played.

// 0x317A08: one draw on six words (engine/original_random.hpp OriginalRandomState::next); the words advance in place.
export function nextWord(w) {
  let value = (w[5] + w[4]) >>> 0;
  let carry = value < w[5] || value < w[4] ? 1 : 0;
  w[4] = value;
  for (let k = 3; k >= 1; --k) {
    const n = (value + w[k] + carry) >>> 0;
    carry = n < w[k] ? 1 : 0;
    value = n;
    w[k] = value;
  }
  value = (value + w[0] + carry) >>> 0;
  w[0] = value;
  w[5] = (w[5] + 1) >>> 0;
  if (w[5] === 0) {
    let k = 4;
    while (k >= 1) {
      w[k] = (w[k] + 1) >>> 0;
      if (w[k] !== 0) break;
      --k;
    }
    if (k === 0) {
      value = (value + 1) >>> 0;
      w[0] = value;
    }
  }
  return value;
}

// The presentation stream of a core (its visual_rng_words); draw() returns BXrand's value, or null without a core.
export function presentationStream(getCore) {
  const words = () => {
    const core = getCore();
    if (!core?._visual_rng_words || !core.HEAPU8) return null;
    return new Uint32Array(core.HEAPU8.buffer, core._visual_rng_words(), 6);
  };
  return {
    available: () => !!words(),
    draw() {
      const w = words();
      return w ? nextWord(w) : null;
    }
  };
}

// 296310: the crash sound of a draw.
export const crashVariant = (r) => 0x30 + Math.floor(((r & 0x7fff) * 3) / 0x7fff);
// 29F660 kinds 0 / 1: the human grunt of a draw.
export const gruntVariant = (kind, r) => Math.floor(((r & 0x7fff) * 3) / 0x7fff) + (kind === 0 ? 3 : 0);
// 2A4CA0: the list entry of a draw.
export const listPick = (list, r) => list[(r >>> 0) % list.length];
