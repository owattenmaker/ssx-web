// Boost pickups across the network (web/pickup_gameplay.inc pickup_revoke). In the original one world a pickup goes
// to the first rider that touches it -- earlier tick first, within a tick the lower slot (the rider passes run in
// slot order) -- and leaves play (60-tick debounce) for everyone else. Online another racer's take reaches this client
// one latency later, so both can take the same pickup inside that window. Every client applies the same rule to the
// same (tick, slot) pairs, so exactly one keeps it: this client gives its take back when a remote take of the same
// pickup was earlier (or the same tick from a lower slot) and within the debounce of its own.
export const DEBOUNCE_TICKS = 60;
export function createPickupArbiter({ slot }) {
  const mine = new Map(); // resource -> tick of this rider's take
  const stats = { takes: 0, conflicts: 0, revoked: 0, kept: 0 };
  return {
    stats,
    // This rider took `resource` at `tick` (its own shared world event kind 2).
    took(resource, tick) { mine.set(resource, tick); stats.takes++; },
    // Another racer's take; returns true when this rider must give its take back.
    remote(resource, tick, from) {
      const t = mine.get(resource);
      if (t == null || Math.abs(t - tick) >= DEBOUNCE_TICKS) return false;
      stats.conflicts++;
      const theirs = tick < t || (tick === t && from < slot);
      if (theirs) { mine.delete(resource); stats.revoked++; return true; }
      stats.kept++; return false;
    },
    // Forget takes older than the debounce (the pickup is back in play).
    prune(now) { for (const [r, t] of mine) if (now - t >= DEBOUNCE_TICKS) mine.delete(r); },
  };
}
