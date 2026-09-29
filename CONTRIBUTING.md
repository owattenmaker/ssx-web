# Contributing

Thanks for helping. Contributions are accepted under the project's licence, GPL-3.0-only ([LICENSE](LICENSE)).
A few rules keep the project legal and faithful:

- **No game data, ever.** Do not commit anything extracted or generated from the disc or the executable: assets,
  audio, movies, text dumps, textures, savestates (`*.p2s`), memory captures, or code machine-translated from the ELF.
  Generated output goes to the git-ignored `web/generated/`, `engine/generated/`, `web/public/assets/` and `local/`.
  Decompilation notes with addresses and short excerpts are fine; bulk dumps are not.
- **Bring your own disc.** Tools take the disc image path as an argument (`--iso`, default
  `~/Downloads/SSX 3 (USA).iso`); do not hard-code personal paths.
- **Parity with the PS2 decides.** Behaviour changes come from the original code (with addresses), are implemented
  exactly, and are checked against the PS2 in an emulator (docs/HANDOFF.md, docs/reference-harness.md). The
  simulation stays bit-exact: `cd web && npm test -- ps2-captures <area>`.
- **No secrets or personal data.** Hosting configuration lives in the git-ignored `deploy/.env.local` (see
  `deploy/.env.example` and docs/hosting.md). Never commit passwords, tokens, logs or player telemetry.
- Run the targeted tests for your change (`cd web && npm test -- <word>`) and `node --check` on edited JS.
