# Asset format references and provenance

The offline decoders were developed with the format research in [GlitcherOG's SSX-Library](https://github.com/GlitcherOG/SSX-Library), pinned at `a3723ab3a3fad1e3c9409c849c0bbd56a290741f` (the submodule of SSX-Collection-Multitool). Reference files include `SSBHandler`, `SDBHandler`, `WorldPatch`, `WorldSSH`, `ByteUtil`, `Refpack`, `SSX3PS2MPF`, `SSX3GCMNF` and `OldShapeHandler`.

The adapted offline decoder/import/comparison files are marked GPL-3.0-only, as upstream (SSX-Library ships the GPL-3.0 text with no "or later" notice); the whole port is GPL-3.0 (`LICENSE`). The licence text is also at `licenses/GPL-3.0.txt`. No upstream C# library, console renderer or CPU runtime is linked into `engine/`.

## Observed layout details

- Both world streams use CBXS/CEND framing with little-endian compressed block extents. Resource lengths/IDs use PS2 little endian and GameCube big endian. Records may span compressed blocks.
- SDB/GDB location records are 88 bytes, after an 80-byte header. The third u32 after the name is the inclusive final chunk index, not the initial index. Platform chunk layouts differ.
- PS2 terrain records are 432 bytes; GameCube records are 430. Both contain 16 float4 bicubic coefficients beginning at offset 64, in reverse power-basis order. PS2 surface points use centimeters in the original Z-up world basis. Native world output converts `(x,y,z)` to `(x,z,-y)` after the original node and instance transforms, including the same rotation for normals; the package declares native Y-up and basis version1. Lighting rectangle is at 16; base UV corners at 32; base/lightmap IDs at 416/418. Later structural fields differ by platform.
- GameCube CMPR world lightmaps use a 32-byte shape header, big-endian dimensions, and four BC1 blocks per 8x8 tile. Endpoints and selector rows follow GC byte/bit order. They are decoded offline to RGBA.
- PS2 paletted textures use a 128-byte header and a second 128-byte palette header. Palettes can have arbitrary used color counts, not just 16/256. Their stored allocation includes alignment and swizzle padding; addressed palette entries must be bounds checked independently of the used-count field.
- PS2 type-5 world lightmap data cannot be treated as final RGB lighting directly: that produced yellow/blue artifacts in the native renderer. Raw decoding remains available for diagnostics, with the mixed GameCube path used for the active build.
- SHPG texture containers use big-endian image count, offsets and dimensions even though some header fields use another order. SHPS equivalents are little endian. BIG members may contain RefPack-compressed texture containers.

Every generated package records its source hash and asset origin. Original data and decoded assets stay under ignored `local/`; none are intended to be committed or distributed with the engine source.

## Original resource names and helper geometry

PS2 BAM.PHM and BAM.PSM contain corresponding resource-ID and null-terminated name arrays: terrain 0, instances 1, models 2, splines 3, collision 4. PHM entries are 16 bytes; PSM string groups align to 4 bytes. The importer validates matching group counts and every extent, preserves original instance/model names, and records both source hashes.

Authored `mdl_...trig...`/`mdl_...trigger...` model identities describe trigger-helper geometry. The known ARA1 slab is `mdl_ARA1_startfireTrig_1000`, model(8,295), instance(8,644). These meshes must not be promoted to physical/rendered scenery simply because they have triangles and materials. The native importer retains their transformed geometry and identity under `trigger_volumes`, while rendering/colliding ordinary scenery separately. Original event binding/execution and non-trigger visibility rules remain unfinished. `collision_sources` records triangle-span provenance for remaining scenery and terrain.

## Native rider pose assets

`rider_assets.py --rider mac|zoe` preserves each MNF primitive group's material;
Zoe TopB requires both suit and tattoo (`eatf`) batches. Each original bone's raw
source translation/quaternion float bits are stored alongside readable values,
so JSON number parsing cannot erase signed zero. Native rendering still uses the
same Y-up conversion as the world. Zoe's selected source bind transforms match the
live PS2 compiled rig exactly, including the secondary hair bones.

PS2 `DATA/CHAR/ANM.BIG` contains little-endian AFL directories (magic0x134C);
GameCube `anmb.big` has big-endian AFB directories (magic0x4213). Float24 curve
coefficients use the same byte layout; full-rate16-bit samples are platform-endian.
`export_animation_samples.py --rider zoe --source ps2` emits original compressed
packets for all497 basic entries plus a small expanded-frame inspector selection.
Overlapping segment boundaries select the next packet's first row, as original
0x311318 does. See `engine/ANIMATION_RECOVERY.md` for pose ordering and conformance.

## Native sky box packages

`tools/import_sky.py` exports the original per-peak sky for an area into the
existing `local/assets/native/<AREA>/` package as `sky.json`, `sky-vertices.bin`
(40-byte vertices: position, normal, uv, lighting uv, already in native Y-up
meters), `sky-colors.bin` (RGBA float per vertex), `sky-indices.bin` (uint32
triangle list) and `sky-textures/9-<rid>.rgba`.

Where the data lives in PS2 `BAM.BIG`:

- BAM.SDB lists five sky locations, `ASKY` (index 10, SSB chunk 36), `BSKY` (17,
  chunk 57), `CSKY` (26, chunk 83), `DSKY` (34, chunk 108) and `ESKY` (47, chunk
  151). GameCube `bam.gdb` has the same names at chunks 39/68/106/144/195 with a
  GC-format model that is not exported.
- Each sky chunk holds nine kind-9 textures (one 256x256 top, eight 256x128
  panels; PS2 8-bit paletted), nine kind-0 materials, one MDR model
  `mdl_?SKY_SkyTop1` (kind 2, 21 meshes, 365 vertices, 323 triangles) and one
  kind-3 instance with an identity matrix, unit scale and 365 baked vertex
  colours (all 16/31, the PS2 `0x80` unity multiplier).
- The decoded model is only 3 m across (x,y +-1.5 m, z -0.79..+0.5 m in source
  units after the /100 conversion). It is not world geometry: the original draw
  routine at SLUS_207.72 `0x353b10` copies the identity matrix at `0x4ff1a0`,
  replaces its translation row with the camera position (`0x2d1c20`), pushes a
  modified render state and draws the instance with flag `0x5420`. The debug
  toggle "Disable Sky Box" lives at `gp+0x143C` (`0x4a452c`). Draw it before the
  world with depth writes off and no culling (mirrored panels reverse the strip
  winding; every vertex normal points inward). Material short 7 is 25 for the
  top, 27 for the lower panels and 31 for the four horizon panels, whose upper
  texels are fully transparent so the dome top shows through them.
- Area to sky mapping: a PCSX2 ARA1 capture shows the SkyBox object
  (vtable `0x48f008`) holding region index 10 = ASKY; the exporter maps every
  area by its peak letter (`ARA1`, `A` -> ASKY, `BRA2` -> BSKY, ...). The
  `mdl_<AREA>_<X>_skybox_trigger` helper instances switch the loaded sky at peak
  transitions (region list loop at `0x22de98`, indices 44..48).

Fog: `sky.json` also carries the area's painted fog. World painter records
(SSB kind 15) start with `0x10, 0x0e, 0x40` and thirteen section offsets for
tWPIGD types 1..13 (5 = Fog, 8 = SkyBox on/off weight, 9 = Sun). A section is
`header_size, count, 0xc, (type, payload_offset) * count`, a spatial quadtree
that is not decoded yet, then 7-float payloads: blend rate, mode, near cm, far
cm, r, g, b. The tWPIGD_Fog constructor defaults are 3000/30000 and
(0.43, 0.55, 0.71). At the ARA1 start the live objects hold entry 0: near 30 m,
far 100 m, colour (0.70, 0.82, 1.00). (The frame is not cleared to it: see below.)

Browser sky clear-color correction: the separate sky scene now clears with the
same recovered start fog color used by the terrain fog. Previously it retained
hard-coded0x7a9cb9 while fog used painted entry0 (0.70,0.82,1.00), leaving an
unrelated color behind uncovered dome pixels. This aligns the two within the
existing browser color pipeline; it does not establish original GS color parity.
**Superseded 2026-09-26** (docs/presentation.md section 7): the PS2 clears to black (382AF0, renderer+6AE0 = 0 in
every in-race savestate); the fog composite then fogs the sky pixels with palette entry 0. The sky scene clears to
black (`pv('skyClear')`).

Near-plane clipping was investigated but not changed: the imported ASKY dome's
minimum triangle-plane distance is0.497425m and minimum vertex radius0.499976m;
the normal chase near plane is0.3m. No normal-view near-plane failure was reproduced.
Spatial fog sampling, skybox weights and peak-transition selection remain open.
Production build passes after the clear-color change; gameplay code is unchanged.

## World texture library (SSB kind 9) and light pages (kind 10)

SSX 3 keeps **one texture table for the whole world stream**; a location does not own its textures. Established from
the disc and SLUS_207.72 (2026-09-25):

- **Counts in BAM.SDB.** Header u16 `+0x2A` = 788 = the number of world textures (kind-9 resource ids 0..787), u16
  `+0x2C` = 623 = the number of light pages (kind-10 ids 0..622). GlitcherOG's SSX-Library `SDBHandler` reads these
  bytes as `UnknownBytes2`; the original code below gives them their meaning.
- **Records.** Every kind-9 (texture, PS2 SHAPE record: 128-byte header, format byte 1/2 = 4/8-bit paletted, 5 =
  RGBA, dimensions at +4/+6) and kind-10 (light page) record sits on the global track 255; the resource id is global.
  Across the 159 SSB chunks there are 6,203 kind-9 records for the 788 ids: every copy of an id is byte-identical, 376
  ids are carried by more than one location (up to 40; e.g. id 17 by 64 exported folders before this change). The 623
  kind-10 ids each belong to exactly one location (light pages are not shared).
- **Registration (code).** The chunk resolver `cPS2ChunkResolve` (name 0x494F70, created at 0x3AAD98) dispatches the
  23 record kinds through the table at 0x494FB0. Kind 9 (0x3AAF5C) calls the renderer (`*(gp+0x2A90)`, vtable at
  +0x10D8, slot +0x170 = 0x37C8C0 in the PS2 renderer vtable 0x493260) with the record, the name "strm_tex"
  (0x494F88) and **handle = the resource id** (record word +4 >> 8). Kind 10 (0x3AAF88) does the same with "strm_lpg"
  (0x494F98) and **handle = resolver+0x10 + id**, where resolver+0x10 is the SDB u16 `+0x2A` (0x3AAE54): light pages
  are handles 788..1410, after the textures.
- **Texture manager.** 0x37C8C0 parses the SHAPE header and creates the GS texture through 0x37CAF8 -> 0x367440 on
  the manager at renderer+0x18F4: an explicit handle takes slot `manager + 8 + 4*handle` (0x367260 allocates a new
  0x58-byte `NBTexInfo` there; there is no lookup of an earlier entry, the same id loaded again simply replaces it),
  handle < 0 takes a free slot (0x367150, free list at +0x1F48, dynamic handles 1500..1999 of the 2000 slots). This is
  the table `docs/wake-recovery.md` found for FX textures (manager+8+handle*4).
- **Use.** A material (kind 0) starts with the s16 texture id; a terrain patch carries the texture id at +416 and the
  light page id at +418. Draws name the handle, so a texture shared by several resident locations is one texture.
- SSX-Library's level extractor (`SSBHandler.LoadAndExtractSSBFromSBD`) follows the same model: kind 9 goes once to
  a shared `Textures/<rid>.png`, kind 10 to a shared `Lightmaps/<rid>.png`, independent of the location folders.

**In the browser** (tools/export_world_textures.py, web/texture-archive.js):

- `web/public/assets/TEXTURES/world.tex` holds all 788 textures, id = resource id, decoded like tools/import_world.py
  (PS2 SHAPE; ids 121 and 625, which the PS2 decoder rejects with "Swizzled texel outside image", take the GameCube
  CMPR copy of the same id, as the per-location packages did). Every package texture of every exported world package
  (5,368 references in 80 packages, 777 distinct ids) was checked texel for texel against it before the per-location
  PNGs (6,662 files) were removed; 11 ids are not used by any exported package.
- World packages (course event packages, peak locations, sky domes, the TRANSP cutscene set) keep their `world.json`
  texture table with `{width, height, ..., pack: '/assets/TEXTURES/world.tex', id}` instead of `path`.
- Light pages: the PS2 light pages the browser draws are already one file per package (`terrain-light-atlas.png`,
  tools/prepare_terrain_render.py). The GameCube lightmaps (`10-<id>`, only sampled with `?originalWorld=0`) are not
  shared on the disc, so they stay per package in `<package>/lightmaps.tex` (same format; an event package holds its
  course and connectors); main.js no longer loads them for the original world/sky materials, which never sample them.
- The library is fetched once per page, alongside the first world package's vertex data (under the loading screen,
  counted by web/downloads.js), and kept as one Blob; each package decodes only the entries it names. Course switches,
  streamed peak locations and cutscene sets reuse it without another request.

## Texture archives (`.tex`)

One file, many textures (tools/texture_archive.py writes and verifies, web/texture-archive.js reads):

```
0    8 bytes   'SSXTEX01'
8    u32       N = index length
12   N bytes   UTF-8 JSON {version: 1, kind, format: 'png', entries: [{id, offset, size, width, height, colours, rgba, ...}]}
               offset: into the payload; rgba: first 16 hex digits of SHA-256 of the decoded RGBA texels
12+N zero padding to a multiple of 16, then the payload: the entries' PNG files back to back
```

- **Payload = lossless PNG, indexed.** Every PS2/GameCube texture has at most 256 distinct RGBA values, so each entry
  is a palette PNG (PLTE + tRNS, 1/2/4/8-bit indices, zlib 9); more than 256 colours would fall back to 8-bit RGBA.
  Measured on the 777 world textures exported before (2026-09-25): per-location RGBA PNGs 7.55 MB, RGBA PNG zlib 9
  6.37 MB, raw RGBA 41.0 MB (gzip -9 6.36 MB, brotli 11 4.87 MB), indexed PNG 4.74 MB (all 788: 4.91 MB; gzip saves 3%,
  so web/server/precompress.mjs keeps no copy). Raw RGBA would also need the 41 MB in memory or a DataTexture upload
  path of its own; GPU block formats (BC/ETC2/ASTC) are lossy and no single one works in every browser.
- **Decoded by the game, not the browser (2026-09-26).** web/png-texels.js `decodePngTexels` turns an entry (indexed
  1/2/4/8-bit with tRNS, or 8-bit RGB/RGBA; filters 0..4; zlib through DecompressionStream) into its exact RGBA bytes;
  web/texture-archive.js `packageTexture` / `archiveTexture` wrap them in a THREE.DataTexture set up like
  TextureLoader's (linear, trilinear mips, generated mipmaps, flipY on; callers set flipY / wrap / colour space), the
  front-end preview decodes the same way in its worker (fe-preview-prepare.js `decodeTextureBlob`) and the crowd's
  animation frames (crowd-2d.js) swap in as decoded texels. A PNG the decoder does not cover (16-bit, grey, interlaced)
  falls back to the browser (<img>, object URL kept until the texture is disposed; ImageBitmap in the preview).
  - **Why:** the archives shipped (2026-09-25) with each entry decoded by the package's old browser decoder (an `<img>`
    from a blob URL revoked on load through THREE.TextureLoader; createImageBitmap 'none'/'none' in the preview), and
    were checked in Chrome only, where every path passes the bytes through. WebKit (measured in the macOS 27 system WebKit, the engine of Safari 27) does not,
    for these indexed PNGs, through WebGPU copyExternalImageToTexture: the revoked-URL `<img>` uploads the palette
    INDICES as grey levels (a 16-colour texture becomes values 0..15, near black; a 256-colour outfit grey-white), a
    decoded `<img>` or an ImageBitmap uploads premultiplied RGB wherever alpha < 255. Metro City in Safari had black
    snow and start gates, silver/pink rider outfits and no crowd. Measured on 30 entries (rider outfits, 16-colour and
    translucent world textures) in the macOS system WebKit: 19 wrong through the revoked `<img>`, 11 through a decoded
    `<img>`, 11 through ImageBitmap; 0 in Chrome; 0 in both through the JS decode. 2D canvas drawImage in WebKit is
    exact (the Equip Gear icons stay `<img>`).
  - **Check:** web/test-texture-archive-decode.mjs (npm test): all 8,786 entries of the 136 archives decode to the
    index digest (3.8 s in node); packageTexture never touches the browser decoder; web/texture-decode-test.html runs
    the game path (packageTexture -> three.js WebGPU upload -> GPU readback) in headless Chrome and in the system WebKit
    (web/webkit-driver.mjs + webkit-driver.swift: a WKWebView driven over stdin, compiled once with swiftc; skipped off
    macOS or with SSX_NO_WEBKIT=1) and asserts the GPU texels equal the digest. It also reports how many the browser
    decoder would have got wrong, so a WebKit that fixes the bug shows up as 0.
  - **Before that (Chrome only):** all 777 world textures and the lightmaps of ARA1 and PEAK2/CRA3 reached WebGPU and
    WebGL2 as the same bytes from the old PNG files and the archive entries, including 514,572 texels with alpha 1..15.
- **Cache.** `/assets/*.tex` are ordinary game files: one entry in the edge cache, revalidated with the ETag on every
  visit (304, no body) instead of hundreds of files.
- **Archive references** in `world.json`: `{pack, id}` (pack package-relative unless it starts with '/'). Rider
  packages already use `archive` for the disc BIG a texture came from; that field is unrelated.

## Rider texture archives (phase 2)

On the disc each character's textures are one archive: PS2 `DATA/CHAR/<X>TXP.BIG` (ALLEG, ELISE, GRIFF, KAORI, MAC,
MOBY, NATE, PSYMO, VIGGO, ZOE, 10.8-23.8 MB; OTHERTXP.BIG 2.4 MB holds the shared and cheat-skin textures), GameCube
`data/char/<x>txn.big`; members are SSH/GSH shapes named `<prefix>_<part>_<variant>` (every Equip Gear variant of the
character plus the item icons). The game picks the members the equipped outfit names (texture rule 0x11BE88 /
0x14B988, docs/characters.md "Equip Gear and outfits"). SSX-Library reads these as plain BIG + SSH containers
(`BIG.cs`, `SSH.cs`); the grouping per character is the disc's own.

The browser mirrors it (tools/export_rider_textures.py, run at the end of every rider exporter), one folder per rider
with the archive split by use (one archive with everything made a Snow Jam first visit download 39 MB of textures
instead of 5.8 MB: the race preloads seven riders; docs/HANDOFF.md 2026-09-25):

- `WARDROBE/<ID>/textures.tex`: the textures of the rider's default outfits, i.e. what the `RIDER_<ID>` race and
  `RIDER_<ID>/fe` Select Character packages draw (computer riders, the human in his default outfit, cutscene actors):
  168 textures for 31 riders, 2.9 MB (0.06-0.12 MB per rider, Sam 0.4 MB with his RIDER_SAM_* outfit packages; Sam's
  painted maps, which no wardrobe stem names, are `package_<texel digest>`).
- `WARDROBE/<ID>/gear.tex`: every other texture the rider can wear (the Equip Gear items): 3,674 textures, 55.4 MB
  (Moby 11.0 MB, Allegra 6.9, Viggo 5.9, Kaori 5.3, Zoe 5.3, Mac 4.2, Elise 4.0, Psymon 3.7, Sam 3.5, Nate 2.8, Griff
  2.8; cheat skins one PDA texture). Read the first time an outfit or the Equip Gear screen needs one of them.
  `--single` puts everything in textures.tex (the literal one-archive-per-rider layout).
- `WARDROBE/<ID>/icons.tex`: the Equip Gear item icons (2,862, 44.9 MB), read only by the Equip Gear screen.
- `WARDROBE/<ID>/textures-xbox.tex` / `gear-xbox.tex` (the Xbox HD set, tools/export_xbox_riders.py, docs/xbox-textures.md
  section 8): the same ids in the same order; 3,759 entries are BC entries instead of PNGs: a 16-byte header ('SXBC', u8 codec
  1 = BC1 / 2 = BC2, u8 domain 1 = 'xbox' full intensity, u16 width, u16 height, 6 zero bytes) and the Xbox's level-0 blocks;
  index rows add `codec`, `domain`, `ps2`, `xbox`; `rgba` is the digest of the decoded texels (web/bc-texels.js `decodeBC`).
  web/texture-archive.js reads them instead of textures.tex / gear.tex while `quality.riderTextures` is 'xbox'; a BC entry
  becomes a CompressedTexture (WebGPU 'texture-compression-bc') or its texels, decoded in web/texture-decode-worker.js.
- id = the wardrobe texture stem (e.g. `zoe_suit_b01_b01`). `wardrobe.json` gains `texture_pack` / `gear_pack` /
  `icon_pack` and `textures[stem].pack`; `web/wardrobe.js` buildPackage gives an Equip Gear package
  `{pack: textures[stem].pack, id: stem}` entries: the same stems, in the same order, as the PNG paths before
  (web/test-texture-archive.mjs builds the default and three gear combinations of every rider both ways).
- `RIDER_<ID>/world.json` and `RIDER_<ID>/fe/world.json` keep their texture tables with `{pack, id}` instead of `path`.
- Each archive is downloaded once, when first needed: the Select Character preview (web/fe-preview.js: the Blob slices
  go to the preview worker, decoded there by web/png-texels.js), a race (the human through main.js asset(), computer riders
  through web/opponent-riders.js), another player's outfit online (wardrobe.js remoteOutfitRider packages), cutscene
  actors (the FE preview packages). The newest eight rider archives stay in memory; an older one needed again is read
  again from the HTTP cache (a revalidation).
- Checked: all 7,138 replaced PNGs (3,837 wardrobe textures, 2,862 icons, 439 RIDER_* package textures) against their
  archive entries (the textures again after the textures/gear split) in Chrome for Testing through both decoders (`<img>` and createImageBitmap without premultiply /
  colour conversion) and WebGPU upload: identical bytes.
