```text
   *              /\                          .         \o/
        *        /::\         *                          |
                /::::\       /\                        _/ \_      .
               /  /\  \     /::\                   . ~=======~
         /\   /  /  \  \   /::::\               .
        /::\ /  /    \  \ /  /\  \           .                *
       /    \  /      \  \  /  \  \        .
      /  /\  \/        \   /    \  \     .                          *
     /  /  \  \         \ /      \  \  .
____/__/____\__\_________V________\__\________________________________

              ███████╗ ███████╗ ██╗  ██╗    ██████╗
              ██╔════╝ ██╔════╝ ╚██╗██╔╝    ╚════██╗
              ███████╗ ███████╗  ╚███╔╝      █████╔╝
              ╚════██║ ╚════██║  ██╔██╗      ╚═══██╗
              ███████║ ███████║ ██╔╝ ██╗    ██████╔╝
              ╚══════╝ ╚══════╝ ╚═╝  ╚═╝    ╚═════╝
           ━━━━━━━━━━━━━  W E B   P O R T  ━━━━━━━━━━━━━
```

# SSX 3 web

A browser port of PS2 SSX 3: the recovered game logic compiled to WebAssembly (`web/*.cpp`, `web/*.inc`, `engine/`)
and a three.js WebGPU renderer (`web/*.js`), with online multiplayer (`web/server/`, `web/net/`).

> **Unofficial fan project.** SSX and SSX 3 are trademarks of Electronic Arts Inc. This project is not affiliated with,
> endorsed or sponsored by Electronic Arts or EA Sports BIG.

- `web/`: the game (Vite app), the WASM core sources, the multiplayer/hosting server and the tests (`npm test`).
- `engine/`: shared recovered engine headers used by the core build (`web/build-core.sh`).
- `tools/`: asset extraction and capture tooling (reads your own SSX 3 disc image).
- `docs/`: design notes; start with [docs/HANDOFF.md](docs/HANDOFF.md). Hosting: [docs/hosting.md](docs/hosting.md).
- `deploy/`: hosting scripts; the host-specific values live in a git-ignored `deploy/.env.local` (see `deploy/.env.example`).

## Bring your own disc

**No game data is in this repository**: no extracted assets, audio, movies, text, executable code or code generated
from the executable. Everything the game needs is made on your machine from **your own** SSX 3 (USA) PS2 disc image
(`SLUS_207.72`). The tools read `~/Downloads/SSX 3 (USA).iso` by default (most take `--iso PATH`). `cd web && npm run
setup` extracts the assets into `web/public/assets/`, writes the generated headers into `web/generated/` and
`engine/generated/` (code lifted from the executable), and builds the core (`web/runtime/`). None of these are ever
committed.

> **Status: setup from your own disc is not finished yet.** Today's assets were built over many sessions with about
> 120 exporters, some of which also read the GameCube version of SSX 3 (course geometry and rider models) and PS2
> emulator savestates (start states, event activation and similar runtime data), and `npm run setup` covers only part
> of that. A one-command pipeline (`tools/setup_from_iso.py`: your PS2 disc, your GameCube disc and a small downloadable
> state pack) is in progress (see [docs/iso-pipeline.md](docs/iso-pipeline.md)); until it lands, expect to need the tools and the
> docs in `docs/` to reproduce the assets.

```sh
cd web
npm install
npm run dev            # local development (after the assets are prepared)
npm run online:build   # hosting bundle -> web/dist-online (served with public/ by server/mp-server.mjs)
```

## License

The whole port is licensed under the **GNU General Public License, version 3** (GPL-3.0-only): see [LICENSE](LICENSE).

- **SSX-Library** ([GlitcherOG/SSX-Library](https://github.com/GlitcherOG/SSX-Library), part of SSX-Collection-Multitool,
  GPL-3.0): the asset-format knowledge behind the offline extraction tools comes from it. The tools adapted from it and
  the Sam PS2 build tools that link it carry `SPDX-License-Identifier: GPL-3.0-only` headers, matching upstream
  (details in `docs/asset-formats.md`). The browser runtime (`engine/`, the WebAssembly core, the renderer, UI, audio
  and networking) does not include or link SSX-Library code.
- **PS2Recomp** ([ran-j/PS2Recomp](https://github.com/ran-j/PS2Recomp), GPL-3.0): the reference tests (`tests/`) and some
  tools compile against its runtime and the code it recompiles from your disc. Neither is included here:
  `tools/bootstrap.py` fetches PS2Recomp and the recompiled code stays in the git-ignored `local/`. Anyone who
  distributes binaries built with it must do so under the GPL-3.0 with the corresponding source.
- **ssxdecomp/ssx3** ([ssxdecomp/ssx3](https://github.com/ssxdecomp/ssx3)) publishes no licence, so none of its code or
  data is copied here. The notes cite it as a reference, and `tools/bootstrap.py` clones it for local use only.
- The optional GameCube research route (`tools/bootstrap_gamecube.py`) fetches DolRecomp and GXRuntime (GPL-3.0) and
  nodtool (Apache-2.0) at setup time; none of them are included.
- npm dependencies (three.js, Vite: MIT) are installed by `npm install`, not vendored.
- **nlohmann/json** 3.10.4 (MIT, `web/third_party/nlohmann/`, with its licence) is vendored for the core build.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).
