> **Superseded (2026-09-23):** every original rider, the twenty cheat characters and Sam are now selectable with their own packages and per-character settings, on the original Select Character screen. See [characters.md](characters.md). The notes below describe the earlier Sam/Zoe-only selection.

# Browser rider selection

Sam and Zoe are now selectable with left/right arrows or the previous/next rider
buttons in the existing character screen. Choosing a rider loads its own original
rig/skin, reinitializes animation against that skeleton, clears pose interpolation,
and disposes the previous mesh/material/texture resources. Switching is held while
assets initialize. Shared animation library metadata is retained across switches.
The selected name appears in setup, details and results.

Zoe uses her27-bone assembly, whose model selection and source bind components were
previously verified against the original Snow Jam capture. Her gameplay uses the
existing recovered Zoe development profile; Sam continues to use that same profile.
This does not implement distinct Sam statistics or the other original riders' full
profiles. Existing character-screen rank/stat bars remain provisional.

Zoe's full biography is copied from kT_FULLBIO1Zoe in the owned FEAMER.LOC; the
roster manifest stores its locale hash. Bio/card text wraps using the extracted
bitmap-font advances. Her equipped texture color variants, hair simulation, morphs
and full wardrobe/equipment rules are still unverified or unfinished. Other imported
rider assemblies remain outside the menu until their integration is handled.

`web/check-camera.mjs --zoe` runs the actual WASM pipeline with her original rig
through charged and held-ledge departure/landing and camera/fog tests. It is now
part of npm test. The full suite and production build pass. Browser checks cover
Sam→Zoe→Sam→Zoe switching, original bio layout, and Zoe starting a race with the
original animation pipeline. This is not a continuous console replay comparison.

The existing finishTest fixture was also exercised through the UI with Zoe:
results show Zoe and the recorded fixture time. This verifies selected-name
propagation, not completion of a full physical race or opponent standings.

Zoe regression coverage now also includes the497-clip start/middle/end finite-pose
audit, all15 grab chords, held-ledge release, combined spin/flip and motion/pose
pipeline, trick names with degrees/flips, crash recovery and all four Uber grinds.
The rail tests validate all27 bones rather than a hard-coded26-bone prefix. These
runs pass and are part of npm test via --zoe; Sam remains the default test rig.
Crash artifacts are separate per rider while retaining Sam's historical filename.
No new gameplay behavior was changed during this coverage pass. These tests do
not prove all original animation triggers, hair motion, wardrobe fidelity or an
entire original-console replay.
