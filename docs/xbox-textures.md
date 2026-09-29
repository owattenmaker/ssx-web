# Xbox SSX 3 as a texture source (evaluation, 2026-09-28)

Owen's own Xbox disc image (the USA disc) was compared texture by
texture with the PS2 data the port draws. **Short answer: only the rider textures gain. Every rider outfit, head, boot,
board and hair texture is on the Xbox at 2x the PS2 resolution per axis, with the same art and UV layout and real extra
detail. Everything else (world, sky, UI, fonts, pictures, gear icons, effects) is the same resolution on the Xbox and
equal or worse.** Owen chose the rider set as an option (Xbox HD on desktops, PS2 on phones, DXT blocks): section 8 (built,
on by default since 2026-09-28, pv xboxRiders).

Evidence (git-ignored, game data): `local/xbox/compare/` (metrics JSON, crops, the analysis scripts in `scripts/`).
The extracted disc is in `local/xbox/` (`SSX 3 (U)/SSX 3 (USA).iso`, `disc/`, `world/`). No Xbox data goes into git.

## 1. Verdict per asset class

| class (what the port uses today) | count | PS2 | Xbox | verdict |
| --- | --- | --- | --- | --- |
| rider textures, `WARDROBE/<ID>/textures.tex` + `gear.tex` (suits, heads, boots, boards, hair / hats, accessories) | 3,777 matched | 8-bit paletted, 64-256 px | DXT1 (opaque) / DXT3 (alpha), **2x per axis** (4x for the 31 board PDA textures) | **better: 3,749**; 4 different layout (keep PS2); 24 no measurable gain; Sam's 65 have no Xbox copy |
| gear icons, `icons.tex` | 2,856 | GameCube-derived, 256x256 | DXT3 256x256 | same resolution, slightly softer and blockier: keep |
| world textures incl. sky domes, `TEXTURES/world.tex` | 788 (same ids) | 476 x 4-bit (16 colours), 308 x 8-bit, 4 x 32-bit | 553 DXT1, 235 DXT3, **same resolution** | equal or worse: keep (see 4.2) |
| UI atlases FE_1 / OV_1 / SU_1 / GL_1 / FL_1 | 35 | 8-bit | DXT5, same size | worse (block artifacts): keep |
| loading logo FLOAD / GLOAD / SPLASH, MENU | 4 | 8-bit / 32-bit | DXT1 / 32-bit, same size | equal or worse: keep |
| effects EFFECTS / PARTICLE / CRWD / BACKS / DOT | 79 | 8-bit, 4-bit, 32-bit | mostly 32-bit, some DXT, same size | 44 are 32-bit copies of 8-bit PS2 ones (PSNR ~50 dB: invisible); crowd 4-bit vs DXT1 (1-bit alpha); keep |
| pictures CHARPIC / COURSPIC / MAPGFX, reward images | 447 | 8-bit | DXT1 / DXT3, same size | worse: keep |
| light pages (SSB kind 10) | PS2 623 / Xbox 455 | 32-bit, <= 128 px | 32-bit, up to 256 px, a different atlas | not interchangeable (different page set, same total texels 7.0 M) |

## 2. The Xbox disc and its formats

- **Image:** `SSX 3 (USA).iso`, 7,825,162,240 bytes: a full redump image, video partition first. The game's XDVDFS
  partition starts at 0x18300000. `tools/xdvdfs.py` (new, read-only) lists and extracts it; layout in its header.
  `data/` mirrors the PS2's `DATA/` (lower case): `char/<x>txx.big` (PS2 `<X>TXP.BIG`), `worlds/bam.big` (bam.xdb /
  bam.xsb / bam.xhm / bam.xsm = PS2 sdb / ssb / phm / psm), `ui/*.xsh` (PS2 `*.SSH`), `textures/*.xsh`,
  `fonts/*.xsh`, `char/mdlxbx.big`, `char/rwrdxbx.big`. BIG archives are the same `BIGF` layout.
- **SHPX shapes** (`tools/xbox_textures.py`, new): the PS2 SHPS / GameCube SHPG framing with magic `SHPX`, little
  endian: `'SHPX'`, u32 size, u32 entry count, 4-byte directory id, then `{4-char name, u32 offset}` per entry. An
  entry is a chain of blocks with a 16-byte header: u8 code, u24 offset of the next block (0 = last), u16 width, u16
  height, 4 x u16 (centre / position), texels. Codes on the disc:

  | code | format | where |
  | --- | --- | --- |
  | 0x60 | DXT1 (BC1) | rider suits / heads / boots / boards, world, pictures, crowd, loading logo |
  | 0x61 | DXT3 (BC2, 4-bit explicit alpha) | rider hair / hats / alpha accessories, gear icons, world textures with alpha, some FX |
  | 0x62 | DXT5 (BC3) | all UI atlases |
  | 0x7D | 32-bit B8G8R8A8, **linear rows** (not NV2A-swizzled: checked on dot.xsh's radial falloff), alpha 0..255 | FX, MENU |
  | 0x78 | 16-bit R5G6B5 | debug dbgirad.xsh only |
  | +0x80 | RefPack-compressed texels | BACKS |
  | 0x70 | long name block (C string) | every entry |

  The PS2 codes are 1 / 2 (4 / 8-bit paletted, GS swizzle, CLUT block 0x21) and 5 (32-bit), GS alpha 0..128.
- **Rider members** hold two entries on the Xbox: the texture (same 4-char name as the PS2's) and a half-size DXT1
  `xx_g` map (gloss / environment mask). Most `_g` maps are black; the board ones (`bd_g`) carry what the PS2 keeps in
  the board texture's alpha (the env-map mask 0..50; correlation 0.994). The port uses neither.
- **World stream:** bam.xsb has the PS2's CBXS / CEND RefPack framing and 8-byte record headers
  (`tools/world_assets.py` reads it unchanged). bam.xdb declares **788 world textures** (u16 +0x2A, same as the PS2)
  and 455 light pages (+0x2C; PS2 623). A kind-9 record is a 128-byte header (+0 code 0x60 / 0x61, +4 u16 width,
  +6 u16 height, +14 u16 >> 12 = extra mip levels) and the DXT data of level 0 and its mips. Every id has the PS2's
  resolution. Kind-10 light pages are 32-bit (code 0x7D), 32-256 px.

## 3. Method

- **Pairs:** rider textures by BIG member name (the wardrobe stem), then the entry with the port's 4-char name; icons
  `<prefix>_icon_NNN`; world textures by resource id; SSH / XSH by entry name and occurrence; pictures by member name.
  The PS2 side is what the port draws: the archive entries of `WARDROBE/*/{textures,gear,icons}.tex` (PS2 RGB at half
  intensity + GameCube alpha, docs/characters.md "Race rider texels") and `TEXTURES/world.tex`; UI / FX / pictures
  decoded from the PS2 disc. Decoders checked against the reference ones (tools/xbox_textures.py vs the numpy
  versions: identical texels; PS2 side equal to `export_career.shps_image` and to the archive digests).
- **Metrics** (luminance in 0..255, the PS2 side scaled by the fitted gain):
  - gain: least-squares Xbox / PS2 per channel at the PS2 size (the Xbox box-filtered down);
  - PSNR and luminance correlation at the PS2 size (layout / art check: corr < 0.9 = different art);
  - **detail above the PS2's resolution** (`beyond`): RMS of the Xbox texture minus its own 2x box-down bilinear-up,
    i.e. the energy in the octave the PS2 texture cannot hold. Baseline per texture (`fake`): the same metric on the
    port texture bilinear-upscaled 2x and BC1-compressed (what an upscaled, recompressed PS2 texture would score:
    interpolation loss plus DXT noise). `excess` = beyond - fake. "Better" = corr >= 0.95, excess >= 1.5 and
    beyond >= 1.3 x fake;
  - mean gradient (sharpness at native size), BC block-edge ratio (mean step across 4-texel block edges / inside
    blocks; 1 = none), distinct colours, alpha levels and alpha MAD against the port's alpha.

## 4. Results

### 4.1 Rider textures: real 2x detail

- 3,777 port textures (textures.tex 168, gear.tex 3,674; minus Sam's 65 without an Xbox copy) matched. Resolution: suits
  256 -> 512, heads / boots / boards / hair / hats 128 -> 256, accessories 64 -> 128, board PDA 32 -> 128.
- **Same art and layout:** gain per channel 2.00 / 1.98 / 1.99 (the Xbox stores full intensity, the PS2 half: GS 128 =
  1.0), mean colour difference < 0.7 PS2 byte; median PSNR 34.9 dB, median correlation 0.997 at the PS2 size.
- **Real detail:** median beyond 7.5-13 by kind against 3.2-5.5 for the upscale baseline; excess median 5.0 (p10 2.6,
  p90 8.7), beyond / fake median 2.4x. By kind (median excess): board PDA 20.4, boards 8.2, boots 7.6, `extz` 8.0,
  suits 5.1, hair / `alph` 4.1, hats 4.1, heads 2.6 (faces are smooth; edges, brows, hair lines sharpen). Examples:
  `crops/representative/` (zoe_suit_b01_b01, rocco_bord_a01, moby_boot_a01_a01, kaori_head_a01, elise_alph_b01, ...);
  every better texture per rider: `crops/rider_sheets/<RIDER>_NN.png` (116 sheets; the port texture | Xbox, the same
  64-texel window at matched scale, `+x` = excess).
- **Alpha:** the port's rider alpha is the GameCube's (3-bit: 0, 36, ..., 255). The Xbox DXT3 alpha has the same
  meaning (0..255 opacity; MAD against the port alpha at the PS2 size median 0.7, p95 3.0 of 255) with 16 levels and
  2x resolution. DXT1 textures are opaque on the Xbox as in the port (2,871 opaque on both).
- **Excluded:** 4 with a different layout on the Xbox (would break the UVs): stretch_boot_a01_a01 (the Xbox texture
  fills the whole square and adds a strap), psymon_extn_01, psymon_extk_01, grommet_extk_01 (items moved).
  24 no measurable gain (mostly flat colours). `crops/riders_different/_real_differences.png`.
- **Where it shows:** texel density on the models (RIDER_ZOE / MAC / KAORI, PS2 textures): suits 110-200 texels/m,
  boards 80-125, boots 200-300, hair 300-440, heads 520-620. On screen: the chase view (PS2 frame
  `runs/air-steer-diag.tick388.png`) is ~85 px/m at 448 lines, ~170 px/m at the port's 896-line cap; Select
  Character (`menus/fe-texture/54-b-select-character-zoe.png`) ~150 / ~300 px/m. So in a race the PS2 suits are already
  ~1 texel a pixel (boards slightly magnified on desktop; phones render fewer lines): little gain. Select Character,
  Equip Gear, the lodge, cutscenes and close replay cameras magnify suits, boards and boots 1.5-3x: the Xbox detail is
  visible there.

### 4.2 World textures: same resolution, DXT instead of palettes

- All 788 ids at the PS2 size; same art (gain 1.00, median PSNR 34.6 dB, correlation 0.986). Different content: 190
  and 419 (32x16 light strips, lit on the Xbox), 717 (tree, alpha edge), a few low-contrast ones.
- The Xbox is softer: mean gradient lower on 545 of 788 (higher on 56); BC block-edge ratio median 1.20, p90 1.45.
- Colour depth: 476 PS2 textures are 16-colour; their Xbox copies have a median 335 colours, so smooth ramps band less
  on the Xbox (`crops/world/_p4_smooth.png`). That is the one Xbox advantage, on low-contrast gradients.
- Alpha precision is worse on the Xbox: the 222 alpha textures are DXT3 (16 levels, median 9.5 used) against a median
  41 alpha levels on the PS2 (GS 0..128).
- Sky domes (ASKY..ESKY, 256x256 + 256x128): 8-bit on the PS2, DXT1 / DXT3 on the Xbox, PSNR 30-46 dB: equal.
- Verdict: no resolution gain; mixed quality at best. Not worth a second world library.

### 4.3 UI, fonts, pictures, icons, effects

- UI atlases (35 matched): DXT5 at the PS2 size, PSNR 31 dB, block-edge ratio up to 1.65 (character art): worse. The
  PS2 has PS2-only entries (PS2 logo, DNAS, Dolby, help); the Xbox has `XBOX` / `art_` ones and different EA art
  (`EA_b`, correlation 0.67-0.73).
- Loading logo 512x512: DXT1 vs 8-bit, PSNR 40 dB: equal. MENU: the same 32-bit texels (PS2 alpha 0..128 = Xbox 0..255).
- Course / character pictures and maps (447): DXT1 / DXT3 at the same size, PSNR 32 dB, slightly softer: worse.
- Gear icons (2,856): DXT3 at the same size as the port's (GameCube-derived) icons, PSNR 34 dB, softer, block ratio 1.30;
  alpha 16 vs 8 levels. Equal at best.
- Effects (79): 44 PS2 8-bit textures are 32-bit on the Xbox (sun / halo / ribbon / fog / ice): PSNR ~45-53 dB against
  the PS2, i.e. the same true-colour source minus the palette; invisible in play. Crowd frames (16) 4-bit vs DXT1: more
  colours, 1-bit alpha instead of 7 levels. The Xbox also has its own effects (glow, pspc, shad, mist, mnav) the PS2
  code never draws.

## 5. Cost of the rider textures

Measured on the 3,708 textures first classed better (the 41 speckle ones in section 6 add ~1%; their "different art"
reading came from the black specks of section 6, not from the Xbox):

| | now (PS2) | Xbox |
| --- | --- | --- |
| texels | 101 M | 405 M |
| download as the port's indexed / RGBA PNG (`texture_archive.png_bytes`) | 52.2 MB | 286 MB (320 MB with optimised RGB/RGBA PNGs): DXT colours do not fit 256-colour palettes |
| download as the Xbox DXT blocks, brotli 11 (gzip 9) | | **110 MB** (123 MB) |
| default outfits, `textures.tex`, all 31 riders | 2.2 MB | 4.5 MB (DXT + brotli); ~0.1 -> ~0.17 MB a rider |
| Equip Gear archives, `gear.tex` | 50 MB | 105 MB (DXT + brotli); Moby 10.4 -> 21.8, Allegra 6.3 -> 13.7, Zoe 5.0 -> 10.3 |

Memory (decoded RGBA; rider textures keep their JS texel copy, the GPU copy has generated mips: 4 x (1 + 4/3) bytes a
texel): a race rider's default outfit 1.2 -> 4.7 MB, so a six-rider race **+21 MB**; each extra outfit seen in the
Equip Gear preview +~3.5 MB. The Blob cache (texture-archive.js keeps the 8 most recent rider archives; Safari holds
Blob data in memory) grows in the worst case (the eight largest gear archives, Equip Gear across many riders) from
~50 MB to ~100 MB; in a race it holds only the small default-outfit archives. Against the measured race footprint on the iOS path (~1.0-1.2 GB, near the ~1-1.5 GB Safari tab limit,
docs/mobile.md), +21 MB in a race is small; the gear Blob cache is the part to cap on phones.

## 6. Black specks on rider hair / hats: the PS2 CLUT past its count (fixed in the exporter, 2026-09-28)

The Xbox comparison showed black specks in the port's rider textures where the Xbox copies are solid, e.g.
arielle_hats_a01_m08 (2,017 texels), deiter_hats_a01_n04, yeti_alph_a01, arielle_alph_a01 (Allegra's default hair).
A first reading ("the PS2 has transparent black there, the port draws it opaque with the GameCube alpha") was wrong.

- **Cause: our PS2 decoder.** An 8-bit SHPS CLUT block (0x21) gives a count (+4 x +6, e.g. 238), but its data holds
  more entries (the block runs to the next block: 248 here), and texels index entries past the count. The GS gets
  the whole 16x16 PSMCT32 CLUT (256 entries) from the block's data. `export_career.shps_image` read only `count`
  entries and filled the rest with (0,0,0,0), so `export_characters.ps2_texel_rgba` (PS2 RGB + GameCube alpha) gave
  those texels RGB 0 under an opaque GameCube alpha: black specks. 535 of the PS2 TXP textures do this (all texels
  within the block's data); 532 of them are port textures (60,065 texels), the other 3 are PS2 icons the port does not use.
- **PS2 evidence** (Select Character, Allegra; derived states only, `local/ps2-capture/menus/speck/`):
  - GS VRAM (`GS.bin` of `allegra.final.p2s`): arielle_alph_a01's uploaded CLUT equals the file bytes for all 238
    counted entries **and** for entries 238..255. The six entries its texels use past the count (240..245) are hair
    browns, `3d281080 37220e5d 4d311080 412b1003 24160880 311c0480` (GS alpha 0x80 = opaque on four).
    arielle_extu_01 and zoe_extu_01 (entry 176) match too.
  - Frames (`ab-vram-crop.png`): VRAM entries 240..245 set to opaque magenta: magenta dots all over Allegra's hair and
    pigtails. Set to magenta with alpha 0: no magenta, only a faint pink from bilinear filtering. As-is: brown hair,
    no dark specks. The CLUT copies in EE RAM are not re-uploaded in this screen (patching them changes nothing).
- **Rider draw state** (37A610 -> 363C20 -> TEST builder 3626D8, material word1: bits 12..19 AREF, 20..21 alpha-test
  mode, 23..24 depth mode; bits 2..6 / 7..11 the ALPHA enums):
  - material flag 0x8000 (set at bind 0x386920 when the material name is `alph` or starts with `ea`: hair / hats,
    whose SSH entry is `alph`, and the `eat*` accessories): ALPHA 0x44 `(Cs - Cd) x As >> 7 + Cd`, ATST GREATER 92,
    AFAIL FB_ONLY, ZTST GEQUAL. Alpha 0 is never drawn; alpha 1..92 blends without writing Z.
  - every other material (suit, head, boot, bord, ext*): ALPHA 0x2A (Cs), ATST ALWAYS: drawn opaque, the texture's alpha
    is not used.
  - The port draws every rider batch opaque with alphaTest 0.35 on the GameCube alpha. With the CLUT fixed, no texel of an
    `alph` / `ea*` texture has PS2 alpha 0 where the port's alpha passes that test (0 of 3,777), so the GameCube alpha
    stays (the reason documented in `ps2_texel_rgba`: the port's alpha test was tuned on it).
- **Fix:** `shps_image(data, gs_clut=True)` reads the 256 entries from the CLUT block (raises if a texel indexes past the
  member's end); `ps2_texel_rgba` uses it. Only RGB changes, only on texels whose CLUT entry is past the count; alpha,
  sizes, ids and every other texel are identical (checked on all 3,777 wardrobe textures). Default `gs_clut=False`
  keeps the old read for the other callers.
- **Same issue elsewhere (not re-exported):** 17 reward pictures (DATA/CHAR/RWRDPS2.BIG, 14,593 texels) have texels
  past the count; `export_career.export_reward_images` still uses the old read. COURSPIC / MAPGFX / CHARPIC have none.
  The world decoder (`world_assets.texture_rgba`) already reads past the count.

## 7. Tools

- `tools/xdvdfs.py list|extract IMAGE ...`: read-only XDVDFS reader (partition base auto-detected).
- `tools/xbox_textures.py FILE.xsh [OUTDIR]`: SHPX lister / decoder (DXT1/3/5, 32-bit, 565, RefPack), world kind-9
  records (`world_texture`), PNG writer. Pure Python like the other offline decoders.
- `local/xbox/compare/scripts/`: the comparison (numpy decoders `xdec.py`, metrics `xcmp.py`, runs `run_*.py`,
  sheets, the BC1 baseline encoder `bc1.py`, `sizes.py`). They need numpy + Pillow (a scratch venv; not in `.venv`).

## 8. The Xbox HD rider set (built 2026-09-28; Owen: an option, Xbox HD on desktops, PS2 on phones, DXT blocks)

**Assets** (`tools/export_xbox_riders.py OUT`; not deployed until the coordinator copies them): `WARDROBE/<ID>/textures-xbox.tex`
and `gear-xbox.tex` for all 31 riders (62 archives, 247.6 MB raw). Each twin holds every id of the PS2 archive in the same order:
- 3,759 entries are the Xbox texture of the same name: the Xbox's own DXT1 / DXT3 blocks (level 0 as on the disc; the Xbox rider
  shapes carry no mips) behind a 16-byte header (web/bc-texels.js: `'SXBC'`, codec 1 / 2, domain 1 = 'xbox', u16 width, u16
  height). Index rows add `codec` ('bc1' / 'bc2'), `domain`, `ps2` [w, h], `xbox` (member / entry); `rgba` = the digest of the
  decoded texels (tools/xbox_textures.py `decode_dxt` rules).
- 83 entries stay the PS2 PNG entry, byte for byte: Sam's 65 (no Xbox member) and the 18 of `KEEP_PS2`: 4 with another layout
  on the Xbox (stretch_boot_a01_a01, psymon_extn_01, psymon_extk_01, grommet_extk_01) and 14 without a measurable gain
  (re-measured against the speckle-fixed PS2 archives: 11 within the upscale + BC1 baseline, 3 flat colours). The other
  "different" readings of the first pass were the black specks of section 6, or small detail differences on the same layout
  (checked by eye: the eatw / ext* pieces with correlation 0.92-0.95).
- icons.tex has no twin (the Xbox icons are the same size and softer).

**Download.** Raw BC blocks compress well only with a large window: the outfit variants of an archive repeat whole blocks.
Zoe's gear-xbox.tex: raw 21.5 MB, gzip 11.5 MB, brotli (quality 9, 16 MB window) 3.1 MB, against 4.9 MB for her PS2 gear.tex
(PNG entries do not compress further). All 62: raw 247.6 MB, gzip 126 MB, brotli 40 MB (PS2: 55.5 MB). A race only reads the
default outfits: textures-xbox.tex 8.9 MB raw / 5.6 MB compressed for all 31 riders (PS2 2.7 MB), about 0.18 MB a rider on the wire.
So `web/server/precompress.mjs` also writes a brotli copy of every `*-xbox.tex` (nothing else changes), and `mp-server.mjs`
serves `.br` to clients that accept br (gzip or raw otherwise; `test-precompress.mjs`).

**Browser** (every step off the main thread except the texture object itself):
- `web/quality.js` `riderTextures` ('ps2' | 'xbox'): `?riders=`, else the saved choice (`ssx3.quality`), else
  `defaultRiderTextures`: with **pv xboxRiders** Xbox HD unless iOS / Android / the low tier, without it PS2.
- `web/texture-archive.js` `riderArchiveUrl`: while 'xbox' is chosen, a `WARDROBE/<ID>/textures.tex` / `gear.tex` reference reads
  its twin; a twin that cannot be read falls back to the PS2 archive (console warning) for the rest of the page. Every consumer
  goes through it: the race human (main.js asset), computer riders (opponent-riders.js), Select Character / Equip Gear / the lodge /
  cutscene actors (fe-preview.js), online outfits (wardrobe.js packages).
- A BC entry goes to the **texture-decode worker** (web/texture-decode-worker.js, job web/texture-decode-job.js; web/worker-guard.js
  handshake and main-thread fallback). With WebGPU and `texture-compression-bc` (`configureTextureDecode`, after the renderer
  init in main.js; Chrome and Safari on Apple-silicon Macs have it) the reply is the stored blocks plus a mip chain (2x2 box filter
  of the decoded level above, re-encoded: `bcMipChain`) and becomes a `CompressedTexture` (BC1 / BC2, trilinear like today).
  Otherwise (WebGL, no feature, `?bc=0`) it is the exact RGBA texels (`decodeBC`) in a DataTexture with generated mips, as today.
  The FE preview worker decodes them itself (fe-preview-prepare.js `decodeTextureBlob(blob, bc)`).
- Texel domain: the entry's `domain` 'xbox' (texture.userData.entryDomain) is full intensity. `web/rider-material.js` (race riders)
  and `web/fe-preview.js` (previews) halve it (x 127.5 instead of x 255 for PS2-domain maps) and the unlit branch doubles the
  halved texel, so the colours equal the PS2's (gain 2.0 in section 4.1); UVs, materials, the alpha test and blending are unchanged.
- Rider archive cache: besides the 8 most recent archives, a 96 MB byte budget (32 MB and 3 archives with the Xbox set on a phone);
  the archive in use is always kept.
- **Option:** Options > Display & Touch (FE and the in-game PDA's More options) gets **Texture set: PS2 / Xbox HD** after PS2
  softness, only with pv xboxRiders (web/fe-options.js `TEXTURE_SET_ROW`); Reset options restores the device default. A change
  rebuilds the front-end previews at once (fe-preview.js `retexture`); race riders change at their next load.

**Checked** (silent: Chrome --mute-audio and WebKit through web/webkit-driver.mjs, the 62 archives in a hard-link mirror of web/):
- `test-texture-archive-decode.mjs` (extended): all 3,759 BC entries decode in JS to the exporter's digest; Zoe's twin through the
  game path (DataTexture and the FE preview decode) equals it, the BC reply keeps the Xbox blocks at level 0 with a full mip chain;
  in Chrome and WebKit (web/bc-texture-test.html) Zoe's and Mac's 12 HD textures upload as BC and every one of their 108 levels
  reads back within 2 of web/bc-texels.js (GPU BC1 thirds round differently from the integer rule; no texel off by more than 2).
- Select Character, Equip Gear, the lodge (Peak 1 free ride + its Equip Gear), a six-rider Snow Jam: the textures load as BC with
  domain 'xbox' in both browsers, the colours match the PS2 set, the Xbox detail shows (`local/xbox/compare/hd/`); `?bc=0` and
  `?backend=webgl` take the RGBA path with the same picture (differences only in the mips).
- Six-rider race, rider textures (35 in the scene): PS2 4.1 MB GPU + 3.1 MB JS copies; Xbox HD with BC 2.35 MB GPU + 2.35 MB JS
  (the blocks and mips); Xbox HD as RGBA (no BC: WebGL, or a phone without the feature) 16.4 MB GPU + 12.3 MB JS. Event load to
  the objectives card: 7.56-7.65 s both sets, both browsers. Frame times over 12-20 s of racing: p50 16.7 ms (Chrome) / 17 ms
  (WebKit), p95 16.7-18 ms, the same with either set (the occasional > 33 ms frame in WebKit happens with both).
- Select Character cycling through six riders: no long task, no frame gap over 32 ms, either set, both browsers. The
  texture-decode worker runs as a worker (`ssxWorkers()`) in both.
- Production build (Vite API, scratch outDir): the texture-decode-worker bundle is emitted.

**Shipped** (2026-09-28, coordinator): the 62 archives copied (sha256 checked), `xboxRiders: true` (Xbox HD by default on desktops).
The host's precompress run writes the `.br` copies. Tests pin the set they check (`test-texture-archive-decode.mjs` the PS2 set
for the PS2 archives, `test-fe-screens.mjs` the rows by what they set), so they pass with the switch either way.

## 9. The rider draw state against the PS2 (pv riderDrawState, on; 2026-09-28)

**From the code.** The rider draw 37A610 sets the renderer's material state per material record (header +0x48 records of 20 bytes);
363C20 turns it into GS registers. The TEST builder 3626D8 takes word1 bits 23..24 (depth mode), 20..21 (alpha-test mode) and
12..19 (AREF) and writes TEST_1 / TEST_2 = ATE | ATST << 1 | AREF << 4 | AFAIL 1 (FB_ONLY) << 12 | ZTE << 16 | ZTST << 17 (mode
0 / 1 = ALWAYS, 2 = LESS, 3 = GREATER; depth mode 1 = GEQUAL). Word1 bits 2..6 pick the ALPHA register from the table at 0x491FB0
(enum 1 = 0x2A: Cs; 5 = 0x44: (Cs - Cd) x As >> 7 + Cd; 7 = 0x48: Cs x As + Cd).
- Records with flag 0x8000 (37A974): ALPHA enum 5 (0x44), alpha-test mode 3 with AREF 92 (ATST GREATER 92), depth mode 1. The flag is
  set when the model is bound (0x386920) for a material named `alph` or starting `ea` (0x4A4398 'alph', then 'e','a'): hair and hats
  (their SSH entry is `alph`) and the `eat*` pieces. Texels blend by their alpha; only those above 92 (of 128) write Z.
- Every other record: ALPHA enum 1 (0x2A), mode 0 (ATST ALWAYS): opaque, the texture's alpha unused.
- A nonzero 4th argument (sp+0x2C) skips the per-material state for one additive (0x48) state for the whole model; not the normal draw.

**PS2 frames** (Select Character, Allegra; derived savestates whose GS VRAM CLUT alpha is changed, `local/ps2-capture/menus/speck/`
s40-*, silent ARMSX2, deterministic: the repacked base gives 0 changed pixels):
- suit CLUT alpha 0: **0 pixels change** (the alpha is not used);
- hair CLUT alpha 0: the hair and pigtails vanish (729 pixels); alpha 128: 89 pixels (the hair's own translucent texels made opaque);
  alpha 64: the hair blends (526 pixels change). `local/xbox/compare/drawstate/ps2-alpha-sheet.png`.
- The port's texture alpha is still the GameCube alpha (section 6): against the PS2 alpha it differs by 0.8 of 255 (median) on the 201
  default-outfit `alph` / `ea*` textures, and 97 % of their texels fall on the same side of 0 / 92.

**Port** (`web/rider-material.js riderDrawState`, used by main.js asset for rider batches, opponent-riders.js and fe-preview.js):
- opaque materials: alphaTest 0 and output alpha 1 (the port composites its canvas over the page, so an alpha 0 pixel would show what
  is behind it; the PS2's framebuffer alpha is not seen);
- `alph` / `ea*`: two passes of the same ranges (a material array: two geometry groups in order), both blended (Cs x a + Cd x (1 - a)):
  first the texels above 92 (doubled texel alpha >= 185, alphaTest) with Z writes, then the others without Z writes. Three cannot
  write Z per fragment in one blended draw; the split differs from the PS2 only where a low-alpha texel of a hair card lies in front of
  a high-alpha one drawn later in the same mesh.
- Same experiments in the port (textures' alpha changed at run time, the preview pose frozen; Chrome and WebKit): suit alpha 0 leaves the
  rider unchanged (before: the suit disappeared); hair alpha 0 hides the hair (as before); hair alpha half blends it.
  `local/xbox/compare/drawstate/experiments.png`, `suit-on-diff.png` (only the animated snowflakes differ).
- Open: blending happens in three's linear colour space (as for every world-pass blend of the port), the GS blends encoded bytes: a
  half-alpha dark hair over a light face comes out lighter than on the PS2. The real textures' alpha is 0 or 128 except at edges, so
  this shows only on hair edges (estimated mean 15-16, p95 42-48 levels on those pixels). Port-wide question, measured in
  [visual-parity.md](visual-parity.md) section 38.
- Checked: `test-rider-draw-state.mjs` (new, in test:all), and with the switch on test-opponent-riders, fe-preview, fe-previews,
  rider-lighting, cutscenes, remote-riders, wardrobe. Six-rider Snow Jam with it on: no errors, frame times as off in both browsers.
- Load cost: WebKit event load, 6 alternating loads each way: median 7636 ms off, 7681 ms on (the spread of either is larger). The
  first 3-run deltas (+0.9-1.7 s) were ordering noise. The blended halves add 8 render pipelines (82 -> 90), all compiled before the
  objectives card by the existing warm path; no long task after the race starts. Switched on 2026-09-28 after Select Character,
  Equip Gear and a six-rider race in Chrome and WebKit.
