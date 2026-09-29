# Building the game data from your own discs

`tools/setup_from_iso.py` makes everything the browser game loads (web/public/assets, web/generated, engine/generated,
the WASM core in web/runtime) from the user's own discs. Owner: the ISO-pipeline agent (2026-09-28).

```sh
python3 tools/setup_from_iso.py --iso "SSX 3 (USA).iso" --gamecube "SSX 3 (USA).rvz" --state-pack ssx3-statepack.zip
cd web && npm ci && npm run dev
```

## 1. Inputs

| Input | What | Why |
|---|---|---|
| `--iso` | PS2 SSX 3 NTSC-U, `SLUS_207.72` (redump #6440: 3,005,415,424 bytes, SHA-1 `667c9b2b...`) | everything; the executable's SHA-1 `77114dfd...` (the one ssxdecomp/ssx3 matches) is the hard gate |
| `--gamecube` | GameCube SSX 3 USA (GXBE69), `.rvz` (needs nodtool), plain `.iso`, or an extracted folder | rider/wardrobe geometry, skins and texture alpha; the world light pages (the default `--lighting gamecube` import); 2 of 788 world textures; the snow tumble frames |
| `--state-pack` | the state pack release (never in git) | the values the exporters read from PS2 savestates and captures (section 3) |
| `--sam` | Sam's private inputs present in the tree | Sam is the port's own rider, not on any disc; without them the roster is the disc's |
| (optional) | Xbox SSX 3 disc | the HD rider textures (tools/export_xbox_riders.py, docs/xbox-textures.md); not part of this pipeline |

Tools: Python 3.10+ (standard library only), node 22.12+ (vite 8), ffmpeg with libx264 (movies; `--no-movies` skips
them), git + network once for Emscripten 6.0.9 (`core-emsdk` installs it into local/vendor/emsdk unless `--emsdk` or
`$EMSDK` points at one) and rabbitizer (`core-venv`, the ELF lift of engine/generated), a host C++ compiler
(`c++`/`$CXX`, or `xcrun clang++` on macOS) for tools/export_lit_instances.py. The core build and every exporter run on
macOS and Linux.

## 2. Steps

The step list is tools/iso_pipeline_steps.py (`--list` prints it, about 900 steps). Order:

1. `disc-*`: tools/extract_disc.py (ISO check; local/disc, local/assets/source/ps2: BAM.BIG members, IRR.DAT,
   MDLPS2.BIG), tools/extract_gamecube.py (local/gamecube/disc subset, local/assets/source/gamecube), tools/statepack.py
   restore.
2. `shared-*`: animation banks, world texture library, irradiance, board trail, FX / impact / snow textures.
3. `riders-*` (natives): Zoe and the five Snow Jam opponents (tools/rider_assets.py, export_animation_samples.py), the
   roster.
4. `loc-<L>-*` for the 17 event locations (tools/locations.py): the native import, the first web package, painters,
   pickups, the event evidence (riding start, race event, countdown audits), initial.json, lighting, the second web
   package (tools/prepare_location.py order; Snow Jam keeps its historical paths). After Snow Jam: Zoe's skin and bind
   rows, the snow flipbooks, the opponent and character packages (the other events' opponent exports read their rigs).
5. `post-<L>-*`: set pieces, flags, UV scroll, LiveComp, particles, stage world, attached pieces, crowd, progress meter,
   sections, opponents / rivals, freestyle rules, weather, the final draw batches (formerly local/peak3-logs/post.sh).
6. Lineups, grid scales, rail teeters, lit instances, the rider screens (FE preview, UI atlases, Select Character,
   wardrobe), UI / career / cutscene / audio / movie exporters, the three peaks and the whole mountain, the exporters
   that write to a scratch root (camera triggers, terrain glint, fog puffs, avalanches: tools/install_export.py copies
   them in), the rider texture archives, then the core (`core-venv`, `core-emsdk`, `core-build` = web/build-core.sh).

Stamps (local/pipeline/stamps) hold each finished step's fingerprint (its command and tool source): a rerun skips
finished, unchanged steps; `--from`, `--only`, `--force` select. Logs: local/pipeline/logs/<step>.log.

## 3. The state pack

About 50 exporters read values the game computes at run time: rail runtime flags, the event seeds (the countdown grid,
the glide start the capture tests are bit-exact against), set-piece load state, RNG words, camera words, CDVD read
latencies. They read them from PCSX2/ARMSX2 savestates (their `eeMemory.bin`) and from capture streams
(tools/ps2_capture.py). Savestates hold the game's and the BIOS's code and cannot be shipped; the pack holds only the
bytes the exporters read:

- **Trace** (maintainers): `setup_from_iso.py --trace --states-from <tree with the savestates> --make-pack FILE` runs
  every step under tools/pipeline_hook (a `sitecustomize` on PYTHONPATH, inert unless `SSX3_HOOK_MODE` is set). The
  hook hands each exporter its savestate member / capture file as a `bytes` subclass that records every slice, index,
  `struct.unpack_from`, `find` and regex match; it logs the files each step reads and writes, existence probes and
  directory listings, and refuses writes into the other tree.
- **Build** (tools/statepack.py): for every traced member, only the footprint bytes that differ from what the user's
  disc gives: the ELF's load segments for `eeMemory.bin` (the executable's code and data where the running game still
  has them), zeros elsewhere. Every entry records its SHA-256/SHA-1/MD5 and what it feeds (steps and their outputs), and
  the pack records each step's source fingerprint.
- **Restore** (users): rebuilds each savestate as a zip whose `eeMemory.bin` is the ELF image plus the pack bytes, the
  capture files as sparse files, never over an existing file, and writes local/statepack/restored.json. The restore
  hook makes those rebuilt buffers hash to the originals' digests (the exporters record state digests as provenance),
  so the exporters run unchanged and their output is byte-identical.
- **Retiring entries:** each entry lists the steps it feeds. When an exporter stops reading a savestate (its value is
  computed from the disc or by the core, docs: the (i)/(ii) ports), rebuild the pack and the entry disappears.
- **Evidence files** (tools/iso_pipeline_steps.py `EVIDENCE`): inputs no pipeline step can make (outputs of the
  recompiled-code oracles or the native engine build); kept whole.

## 4. Verification

`tools/verify_assets.py BUILT REFERENCE` compares two trees file by file and classifies each difference (JSON value
paths, texture archive entries by texel digest, PNG pixels). The pipeline was proven on a code-only scratch checkout
(section 5).

## 5. Status

See docs/HANDOFF.md (2026-09-28, ISO pipeline) for the verify results and the list of files that differ from the live
tree and why.
