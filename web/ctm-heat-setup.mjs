// CTM Next heat in the world (pv eventReturnInWorld, docs/ctm-events-in-world.md stage 5 "WS13"): the markers of a capture that runs
// through the results' Next heat (local/ctm-events/caps/c0a-ws13), shared by compare-ai-capture.mjs --ws13.
//   S  the auto replay's first record (the game tick restarts after the live stop; S - 1 is the live stop)
//   W  WS13's first record (control 13 under the gondola NIS, tick 0: 230180's 1297C8)
//   G  1289F0's record (the tick restarts: the semi's riders on the grid, C+0x14 = 1: they tick as in a countdown)
//   C2 the semi's countdown (the card's Continue, 1297C8(C, 1): the tick restarts again)
export function planHeat({ records, C }) {
  const restart = (from) => { for (let k = from; k < records.length; k++) if (records[k].tick < records[k - 1].tick) return k; return -1; };
  const S = restart(C + 2);
  let W = -1; for (let k = S + 1; k < records.length; k++) if (records[k].control === 13 && records[k].tick === 0 && records[k - 1].control !== 13) { W = k; break; }
  const G = W > 0 ? restart(W + 1) : -1, C2 = G > 0 ? restart(G + 1) : -1;
  if (S < 0 || W < 0 || G < 0 || C2 < 0) throw new Error(`--ws13: replay ${S}, WS13 ${W}, grid ${G}, semi countdown ${C2} not all found`);
  return { S, W, G, C2 };
}
