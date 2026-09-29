// The grid spot of an online racer in countdown slot k (docs/characters.md "Grid spot per rider", "Computer-rider
// lineups"). In the original a rider's countdown ground state is a function of its grid slot, its body scale and its
// base rider's stance: the lineup data (tools/export_lineups.py -> <course>/lineups.json) holds the parts:
//   human_grid[scale] + human_template + human_base[base]  the rider's full state on slot 0 (lineup.js humanGridState)
//   grid[k][scale]   the spot of slot k for that body scale (~90 cm x scale along the line): position, ground frame
//   slot[k]          the slot's own words (heading offset); moment[k] / state[k] its countdown moment words
// and a slot-k state differs from the same rider's slot-0 state only in the ground.state paths of those parts. So an online
// racer in slot k starts on slot k's spot for its own scale and stance, as a rider of that scale would in the
// original. test-mp-grid.mjs rebuilds the five anchor computer riders' states this way, bit for bit.
// Scales no computer rider has (the cheat skins outside 0.7..1.0: Canhuck 0.7, Stretch 1.2, Bunny San / Churchill
// 1.3, North West Legend 1.5, Far East Myth 2.0) come from PS2 countdowns with those skins in every slot
// (tools/export_grid_scales.py -> <course>/grid-scales.json). Only recorded spots are used.
import { humanGridState, f32key } from '../lineup.js';

const clone = (v) => JSON.parse(JSON.stringify(v));

// The recorded grid part of slot k for a body scale (lineups.json, else grid-scales.json), or null.
export function slotGrid(lineups, slot, scale, gridScales = null) {
  const key = f32key(scale), part = lineups.grid[String(slot)]?.[key] ?? gridScales?.grid?.[String(slot)]?.[key];
  return part ? { part, exact: true } : null;
}

// The full countdown ground state of `rider` (a riders.json entry; kind/base for a cheat skin) of base character
// `humanBase` and body scale `scale` in grid slot `slot` (0..5) of the course whose lineup data (and grid-scales
// data) is given. Returns {state, exact: true} or null when the course has no recorded spot for it.
export function onlineGridState({ lineups, gridScales = null, rider, humanBase, scale, slot }) {
  if (!lineups || !(slot >= 0)) return null;
  // This rider on slot 0: every scale/stance-dependent field (the body scale it really rides with, else its roster scale).
  const byScale = lineups.human_grid?.[f32key(scale)] ? { id: '#scale' } : rider;
  const own = humanGridState(byScale.id === '#scale' ? { ...lineups, human_scale: { ...lineups.human_scale, '#scale': f32key(scale) } } : lineups, byScale, humanBase);
  if (!own) return null;
  if (slot === 0) return { state: own, exact: true };
  const k = String(slot), grid = slotGrid(lineups, slot, scale, gridScales); if (!grid || !lineups.slot[k]) return null;
  // The slot-dependent parts in lineup.js assembleLineup order (slot, grid; moment and state: the anchor's countdown
  // words), their ground.state paths over the rider's own state.
  const state = clone(own);
  for (const part of [lineups.slot[k], grid.part, lineups.moment?.[k], lineups.state?.[k]]) {
    for (const [path, value] of Object.entries(part ?? {})) {
      if (!path.startsWith('ground.state.')) continue;
      const keys = path.slice(13).split('.'), last = keys.pop(); let o = state; for (const key of keys) o = o[key];
      o[last] = clone(value);
    }
  }
  return { state, exact: true };
}
