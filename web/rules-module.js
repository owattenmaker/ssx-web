// The rules module (docs/core-migration.md M5, pv rulesModule): web/runtime/rules.{js,wasm} (web/build-rules.sh), a small wasm loaded
// at boot, before any course core, with the same rules as the course core (engine/game_rules.hpp) and the session's presentation
// generator (engine/presentation_rules.hpp). loadRulesModule() starts the load once (main.js at boot; node: RULES_JS=<path>); until it
// has loaded, or when it fails, rulesModule() is null and the JS versions run.
let loaded = null;
let loading = null;
export function loadRulesModule() {
  if (loading) return loading;
  // (a literal path for the page, so the bundler hashes the module and its wasm like the course core's)
  const scratch = globalThis.process?.env?.RULES_JS;
  const load = scratch ? import('node:url').then((u) => import(u.pathToFileURL(scratch).href)) : import('./runtime/rules.js');
  loading = load
    .then((m) => m.default())
    .then((m) => {
      loaded = m;
      return m;
    })
    .catch((e) => {
      console.warn('Rules module unavailable', e);
      return null;
    });
  return loading;
}
export function rulesModule() {
  return loaded;
}
