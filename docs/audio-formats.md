# SSX 3 audio formats (PS2)

The layouts below come from the disc data and the code that reads it. EE addresses are in
`SLUS_207.72` (file offset = address − 0xFF000; recompiled in `local/output/sub_*.cpp`). IRX
addresses are in `DATA/MODULES/SNDDRV.IRX`, an IOP ELF that still has its symbol table. Lines
marked **(observed)** are true for every file on the disc but were not traced in code.

Tools: `tools/export_audio.py` writes the assets to `web/public/assets/AUDIO/`.
`web/audio-decode.js` decodes them, and `web/test-audio-decode.mjs` tests the decoders.

## 1. Files

| Disc file | Contents |
|---|---|
| `DATA/AUDIO/MUSIC.BIG`, `MUSIC2.BIG` | per song: a Pathfinder `.mpf`, the stream file `<song>.mus` (EA SCxl streams) and the loop bank `<xx>loops0.mus` (a BNKl bank). Each archive also has a `*.bnk` entry of size 0. |
| `DATA/AUDIO/ENGLISH.BIG` | DJ Atomika and the PA announcer: 29 `DJ_*_eng.dat` / `PA_*_eng.dat`, plus the nested `langhead.big` that holds their `.hdr` line tables |
| `DATA/AUDIO/SPEECH.BIG` | character speech: 294 `<Category>_<chr>.dat`, plus the nested `headers.big` for their `.hdr` tables |
| `DATA/AUDIO/AUDIO.BIG` | 131 BNKl banks (menu, board, world ambiences, crowd) and 9 crowd MIDI files (`*.eam`, magic `MIDx`) |
| `DATA/AUDIO/GRNT_*.BNK` | character grunt banks (MicroTalk) |
| `DATA/CONFIG/MUSIC.INF`, `PLAYLIST.INF`, `MIX.INF`, `BANKS.INF`, `CROWD.INF`, `SPEECH.INF` | song parameters, the default playlist, mixer presets, bank and crowd lists |

The EE loads speech through these strings: `%sheaders.big` 0x4832D8, `%slanghead.big` 0x4832E8
(both used in 0x2B0088), `%s%s%s.dat` 0x4832F8 (0x2B04D8) and `%s.hdr` 0x4A37C0 (0x2AF960, which
creates a `SpeechBank`).

## 2. BIGF archive

```
+0  'BIGF'
+4  u32 LE   archive size
+8  u32 BE   entry count
+12 u32 BE   header size
+16 entries: u32 BE offset, u32 BE size, NUL-terminated name ("data\audio\go.mus")
```

Offsets are relative to the archive start.

## 3. PT header (EA "SND" tag list)

Streams (`SCHl`) and bank patches both start with `"PT", u8 platform (5 = PS2), 0`, followed by
a tag list. The iterator is at EE **0x3C7388** and the big-endian value reader at 0x3C72F8.

- `0xFC`: padding, skipped.
- `0xFF`: end of the header.
- `0xFD`: the sample part follows.
- `0xFE`: end of this patch; the next patch follows. Bank entries can hold several patches.
- Anything else is `tag, u8 len, len bytes`. The value is big-endian. If `len` is 0xFF, a u32 BE
  length follows instead.

Tags below 0x80 are patch parameters: volume, pan, pitch, envelope and so on. They are exported
raw. Sample tags:

| tag | meaning | default (stream 0x3BB820 / bank 0x3BAAC0) |
|---|---|---|
| 0x80 | format version (2 = streams and loop banks, 3 = SFX banks); MicroTalk stream voice: < 3 = plain MicroTalk (section 4) | |
| 0x82 | channels | 1 |
| 0x84 | sample rate | **22050** (0x3BB930 / 0x3BABC0) |
| 0x85 | sample count per channel | |
| 0x86 / 0x87 | loop start / loop end, inclusive, in samples | −1 |
| 0x88, 0x89 | absolute data offset of channel 0 / channel 1 in the bank (planar) | |
| 0x8A | always 0 | |
| 0x8C | 4 (256 on some PCM8 patches) | 8 |
| 0x9C / 0x9D | the same values on every segment of a song (Go: 61348 / 4186); meaning unknown | |
| 0xA0 | codec2: **4** MicroTalk, **5** PS-ADPCM (VAG), **9** PCM8, **10** EA-XA | **5** |

A third parser, 0x3BBDA0, defaults to 48000 Hz and codec 10. It is called from 0x3B6300 and
0x3B6788. Every stream on the disc carries an explicit 0x84 or is decoded with the 0x3BB820
defaults.

## 4. SCxl streams (speech lines and music segments)

A stream is a run of little-endian chunks, each `fourcc, u32 size (header included)`:

```
SCHl  "PT" header (section 3)
SCCl  u32 number of SCDl chunks
SCDl  u32 samples in this chunk
      u32 x channels : channel data offsets, relative to the end of this offset table
      per channel: 4 bytes (two s16 LE values), then EA-XA frames (15 bytes = 28 samples)
      (MicroTalk: 1 skipped byte, then the channel's plain MicroTalk bits; see below)
[SCLl u32 loop chunk]   (not present on this disc)
SCEl  end
```

The EE dispatches `SCDl` to 0x3B6EA8 and `SCHl` to 0x3B6BD0 (0x3B7080), and checks `SCEl` at
0x3B76E8. Streams are zero-padded: `.mus` streams start on 0x80-byte boundaries, and speech
lines start on the alignment given by the `.hdr` (section 7).

**Per-channel 4-byte prefix (observed).** These bytes are close to, but not equal to, the last
two decoded samples of the previous chunk, under every rounding variant tried. They are the
encoder's source PCM, so the decoder ignores them and carries its ADPCM history across chunks.
The IOP filter (`SFILTER_unpackxaf`, IRX 0x9390) zeroes its history only at init
(`SFILTER_unpackxafinit`, 0x95D8). It also addresses frames as `base + pos*15/28`, so the EE
must hand it prefix-free frame data.

**MicroTalk streams (codec2 4; only "Screw Up", `screwup.mus`, 261 bars, 2 channels, 32000 Hz, tag 0x80 = 2).**
The music stream voice is the IOP-mixed voice type 2 of codec 4 (`0x3C9960` open, `0x3C96F0` read, `0x3C9938` close; the
voice types are one generated function `0x3C92F8..0x3C9DA0`). Per SCDl block and channel:

- `0x3B6EA8` builds the block descriptor: sample count with **bit 31 = "not the first block of this SCHl"** (stream
  `+0x24`, cleared by the SCHl handler `0x3B6BD0`, set after every SCDl), and the channel pointer = end of the offset
  table + offset[c]. The ring-buffer fetch `0x3C6DE0` returns the pointer, the count (bit 31 cleared) and bit 31.
- The read `0x3C96F0` opens the next block with `0x3CDDE8(ctx, pointer + 1, ...)`: **the first byte of each channel's
  block data is skipped** (it is `01` in the first block, `00` after).
- Bit 31 clear: `0x3CE0B8` with `ctx+0xD5C = 1` → `0x3CD6F0` parses the 15-bit header and clears the reflection
  coefficients, synthesis history and adaptive codebook. Bit 31 set: `ctx+0xD5C = 0` → only the bit reader restarts;
  **the decoder state carries across blocks** (never across SCHl streams, so every bar decodes on its own).
- Mode: `voice+0x30` = the stream's **tag 0x80** (header struct `+7`, `0x3BB820`; not 0x8C, which is `+2`). Below 3
  (every stream on the disc) `0x3CE0D8(ctx, 0)`: `ctx+0xD64 = 0`, **plain MicroTalk**, no flag bytes, no PCM patches and
  no byte re-alignment between frames (`0x3CDF34`). The EA wrapper (flag byte `0xEE`, patches) is only used by the SND
  bank voices (tag 0x8C >= 3) and by streams with tag 0x80 >= 3 (none).
- Blocks hold whole frames (2160 = 5 x 432 samples, 1728 = 4 x 432); a partly used last frame is dropped when the next
  block opens (`ctx+0xD44 = 0`). Each channel's bits end within 0-4 bytes of the next channel's data.
- The live PS2 state agrees: in the Metro-City savestates (`local/reference/pcsx2/metro-city*.p2s`) both stream voices
  have `voice+0x30 = 2`, `ctx+0xD64 = 0`, `ctx+0xD5C = 0` mid-bar and 1 at the first block.

Reading these blocks as EA-wrapped bank data (a fresh decoder per block, the first byte as a flag) gives full-scale
noise (RMS 0.82, mean |step| 0.24); the correct decode has RMS 0.28 (EA-XA songs 0.2-0.3), peak 0.65-1.0.

**Checks.** The SCDl sample totals equal tag 0x85 for all 9806 speech lines. Consecutive music
segments join without a discontinuity: the jump at each join is at most 0.003, against a typical
step of 0.013–0.036.

## 5. Codecs

### EA-XA (codec2 10): speech, music, loop banks, some SFX

Frame: `u8 header, 14 bytes` = 28 samples, high nibble first. The header's high nibble is the
coefficient pair and its low nibble is `shift − 8`. The IRX decoder is `decxa16c` (**0x9BA0**).
Its table `xafp` (0xACC0) holds the pairs (0,0), (240,0), (460,−208), (392,−220).

```
s = ((nibble << 28) >> (lo + 8)) + h1*c1 + h2*c2
s >>= 8                  // arithmetic shift: no +128 rounding, no clamp in the history
h2 = h1; h1 = s          // output is clamped to 16 bits only when mixed
```

No frame on the disc uses the later 0xEE "raw PCM frame" form, and the IRX cannot decode it: no
header byte is above 0x3F. The EE has no EA-XA table, so all EA-XA decoding runs on the IOP.
Channels are planar: SCDl offsets in streams, 0x88/0x89 in banks.

### PS-ADPCM / VAG (codec2 5, the default): SPU banks, menu sounds

These are standard SPU frames: `[filter<<4 | shift][flags][14 bytes]`, low nibble first,
coefficients f0 = {0, 60, 115, 98, 122} and f1 = {0, 0, −52, −55, −60}:
`s = (nib<<12 >> shift) + ((h1*f0 + h2*f1 + 32) >> 6)`, clamped to 16 bits. Bank data begins
with the usual all-zero 16-byte frame, which counts toward 0x85. The last frame carries flag
bit 0 (end); this was checked on every VAG patch in the test. Rates vary per sample, from 8000
to 44100 Hz (for example 15992 or 16055).

### PCM8 (codec2 9): `zboard.bnk`

The data is signed 8-bit. Main-RAM PCM8 data starts with a 16-byte header:
`00 09 00 00, u32 loop start (−1 if none), u32 loop end, u32 count`. The IRX has
`MIXI_initunpack8` and `SNDI_decode8to32` for it.

### MicroTalk / UTK (codec2 4): grunt banks, most AUDIO.BIG ambiences

This is EA's CELP-style speech codec, decoded on the EE (it is not in the IRX). A frame is 432
samples. Traced functions:

- 0x3CD1F8 / 0x3CD260: bit reader. It reads LSB-first and keeps 8–15 bits buffered, refilling
  one byte when fewer than 8 remain.
- 0x3CD6F0: init and header, 15 bits: `reduced_bw:1`, `threshold = 32 − read(4)`,
  `gain[0] = 8·(1 + read(4))`, `gain[i] = gain[i−1]·(1.04 + 0.001·read(6))` for 64 gains.
- 0x3CD878: frame decoder.
  - 12 reflection coefficients: indices of 6,6,6,6 bits then eight 5-bit indices (+16) into the
    64-entry table at **0x44E8E8**.
  - Multipulse mode applies when the first index is below `threshold`.
  - Four 108-sample subframes, each with `pitch lag:8`, `pitch gain:4/15` and
    `fixed gain index:6`.
- 0x3CD2B0: excitation. Multipulse mode uses a Huffman lookup (codebooks **0x44E9E8**, commands
  **0x44EBE8**: {next model, code size, pulse}) with zero runs of 7–70 and escape pulses of 7 or
  more. The other mode (RELP) codes 0/±2.
- Reduced bandwidth: an `align` bit and a `zero` bit per subframe. Either the other phase is
  zeroed, or it is interpolated (0x3CD518, coefficients 0.5973859, −0.1145916, 0.0180327) and
  the fixed gain is halved.
- 0x3CD590: reflection to LPC coefficients. 0x3CE410: 12-tap synthesis, run as 1+1+1 blocks of
  12 samples with the coefficients interpolated in quarters, then 33 blocks. The 324-float
  adaptive codebook is refreshed from the frame.
- **EA wrapper** (0x3CDDE8 / 0x3CDE68, `ctx+0xD64 = 1`):
  - Every frame is preceded by a flag byte; 0xEE means a PCM patch follows the frame.
  - The patch is `BE16 offset, BE16 count, count × BE16 samples`, written over the decoded
    frame.
  - After each frame the bit reader restarts on the next byte.
  - The first flag byte is followed by the header bits.
- **Plain mode** (`ctx+0xD64 = 0`, set by `0x3CE0D8(ctx, 0)`): no flag bytes, no patches, the bit reader simply runs
  on from frame to frame. Only the music stream voice uses it (section 4, MicroTalk streams).
- **Looped sounds (observed)** have two independent streams. The intro is
  `ceil(loopStart/432)` frames, of which `loopStart` samples are used. The loop body covers
  samples `[loopStart, loopEnd]` and has its own flag byte and header at `dataOffsets[0]` +
  param **0x1A** (channel 0) or + param **0x26** (channel 1). Samples after `loopEnd` are not
  stored: `Transport.bnk` 16 and 17 have 4987 and 1687 of them.
- **Short intros (verified against the original, 2026-09-23).** Eight intros shorter than one frame (GoGo1/2,
  Gravelpit, Port, MineShaft L/R, both copies of Heavy winds) have a header whose bandwidth bit is 0 although their
  parse does not end on the body. The original decodes them with the header as stored (the body is addressed by its
  own offset, the SND voice `0x3C9420` / loop restart `0x3C9520` open it as a fresh stream), and so does the port.
  MineShaft channel 1's misparsed PCM patch (offset 223, count 262) runs past the 432-sample frame; `0x3CDE68` writes
  it without bounds over the context fields after the frame (remaining count, output pointer), so the original loses
  that voice's output: the port writes silence from that frame on.
- **Exact arithmetic.** The decoder uses the EE FPU operations of the original in its order: MUL.S rounds toward
  zero, ADD.S / SUB.S keep one guard bit, the synthesis filter `0x3CE410` accumulates with ADDA.S / MADDA.S (one
  chop per step) in the scheduled order (for output j >= 1: lpc[1..j-1] x older outputs, lpc[j..11] x history,
  lpc[0] x the newest output last), pitch gain = cvt(v) x 0x3D888889, gain base = v x 0x3A83126F + 0x3F851EB8.
  A first-subframe pitch lag above 216 reads the 39 floats before the adaptive codebook, which in the context are
  the synthesis history, the reflection coefficients and the last fixed gains (`ctx+0x14 / 0x114 / 0x144`).
- **Bank JSON keys.** Patch params are keyed `'0x1a'` in the exported JSON and by number from `parseBank`; the loop
  body offset must be read from either (before 2026-09-23 the exported banks decoded the intro stream straight into
  the body and produced non-finite samples in 196 patches, which silenced WebKit's whole graph).

**Checks.** All 192 MicroTalk streams (grunts, SFX banks, loop intros and bodies) parse to within
0–16 padding bytes of the next data. The three tables in `audio-decode.js` equal the executable
bytes, and the check script compares them. `web/test-microtalk.mjs`: all 593 exported MicroTalk bank patches decode
finite, and all 812 streams (61.5 M samples) equal the original decoder (the recompiled `0x3CDDB8/0x3CDDE8/0x3CDE68`
run on the same bytes by `tools/test_microtalk_native.py`) sample for sample. The decoder costs ~0.4 us per sample, so
`web/sfx.js` decodes MicroTalk banks in `web/audio-decode-worker.js` before the bank becomes playable.
Music streams (2026-09-25): all 522 bar channels of "Screw Up" (31.3 M samples) equal the original stream voice
(`0x3C96F0` in `sub_003C92F8`, entered through an added label, with `0x3C6DE0` / `0x3C7010` stubbed to hand out the SCDl
blocks as `0x3B6EA8` describes them, reads of 1000 samples) sample for sample. Against ARMSX2: the decoded frame buffer
`ctx+0x684` of both stream voices in five Metro-City savestates matches exactly one frame of the port (the frame the
voice position `ctx+0xD54 / 0xD44` points at) within 0.006-0.11 LSB of 16 bits (the emulator's FPU rounding differs
from the oracle's scalar model in the last bits; the next best frame is thousands of LSB away). A bar costs ~40-50 ms
to decode, so `web/pathfinder.js` decodes MicroTalk bars in the worker when the player commits them (~500 ms ahead)
and prefetches the opening bar before the song starts (`prefetchSongStart`).

## 6. BNKl v5 banks

```
+0  'BNKl'   +4 u8 version (5)   +5 u8 flags   +6 u16 entry count
+8  u32 header size = start of the sample data
+12 u32 SPU (VAG) data size     +16 u32 main-RAM (IOP/EE-decoded) data size
+20 u32 x count: offset of the entry's PT header, relative to the slot itself (0 = empty slot)
```

For example, `zboard.bnk` = 0xDA0 + 0x40660 and `zboardSPU.bnk` = 0x750 + 0x3A8A0; each sum is
the file size. An entry is a PT header with one or more patches separated by `0xFE`. The bank
patch parser is 0x3BAAC0, and it returns at each 0xFE. Examples of multi-patch entries are
random or velocity layers such as `zboard.bnk` 35/36 and `SSX3Menu.bnk` 3. Data offsets
0x88/0x89 are absolute within the bank file. Patches can share data.

Codec use (patches): PS-ADPCM 230, MicroTalk 192, EA-XA 18, PCM8 27. The music loop banks
(`*loops0.mus`, `Peak*_Ovr0.mus`) add 1704 EA-XA patches at 22050 Hz. Those are 1-beat slices
plus a few long endings.

## 7. Speech `.dat` + `.hdr` line tables {#speech-hdr}

A `.dat` is a plain concatenation of SCxl streams, one per line. It has no internal index. The
index is the matching `.hdr` inside `langhead.big` (DJ/PA) or `headers.big` (characters). Its
parser is at EE **0x3D69F0**.

```
+0  u16  id / hash
+2  u16  0xFFFF
+4  u8   bits 0-3: field count F (0..2); bit 7: extra "played" bitmap follows
+5  u8   line count N
+6  u8   history length H (normally N)
+7  u8   alignment mask M: line offsets are in units of 256·(M+1) bytes (M = 0, 1, 3, 7)
+8  u32  .dat size / 256
+12 N entries of (2 + F) bytes: u16 BE offset (units above), then F selection-key bytes
    padding to 4 ('p' = 0x70), then F × u32 masks of the key values present
    u8 history cursor, H bytes of recently played line numbers (0xFF = empty)
```

This table maps all 9806 lines on the disc: 8399 mono and 1407 stereo (DJ lines), all EA-XA at
22050 Hz, 8.4 hours in total. No stream is unindexed.

The speech streams carry no 0x84 tag, and the rate comes from the stream default. An
autocorrelation pitch check agrees: DJ lines have F0 ≈ 150–160 Hz and Allegra ≈ 300–330 Hz at
22050 Hz, which would be implausible at 48000 Hz.

**Addressing.** A line is addressed by its table index. The key bytes group the lines. For
example, `DJ_Aggression` has one key with 5 groups of 3 variants. `DJ_Event_Intro` has two keys,
(event, variant), such as (0,0), (0,1), (0,1), (1,0) and so on. The masks, the history and the
`played` bit suggest that the game picks randomly among the lines matching the requested keys
and avoids recently played ones. The selection logic is not traced here.

## 8. Music

### MUSIC.INF

Each song section has TITLE, ARTIST, ALBUM, PATHDATA (.mpf), MUSDATA (stream .mus), LOOPDATA
(loop bank), BPM, BeatsPerMeasure, MeasuresPerBar, PhrasesPerBank ("# phrases in each async
bank"), BeatsPerPhrase, PathLevel / AsyncLevel (%), CATEGORY (repeated: 0 Race, 1 SlopeStyle,
2 BigAir, 3 HalfPipe, 4 BackCountry), DUCKTOLOOPS, SEDVALUE, LOWPASS, PREVIEW, SONGBIG (1 or 2)
and ADDTOFE. The file's own comments give the defaults; the exporter applies them.
`PLAYLIST.INF` gives the default "SSX Mix" of 35 songs, and `MIX.INF` gives 3 mixer presets:
MUSIC, DJ, PA, CHARACTER, BOARD, COLLISION, AMBIENT, ARCADESFX and ARCADESPEECH volumes plus a
TIME in ms. Titles and artists are in MUSIC.INF, not the executable.

The 45 Pathfinder songs are 35 licensed tracks plus `charsel`, `chartune`, `Peak1–3` (hub
spokes), `Peak1–3Amb`, `pktrans` and `mapsel`.

### `.mpf` Pathfinder v4

The magic bytes are `78 44 46 50` ("PFDx" as a LE u32), followed by the version byte 4. The
loader, **0x3D2350**, checks the magic at 0x3D240C and the version at 0x3D2420, then builds the
table pointers at 0x3D2474–0x3D256C. All offsets are ×4 from the file start.

```
+0x04 u8 version (4)    +0x06 u16 (0x08B0 songs, 0x063C ambiences)
+0x0D u8 tracks  +0x0E u8 sections  +0x0F u8 events  +0x10 u8 routers  +0x11 u8 vars
+0x12 u16 nodes          +0x14..0x1F zero
+0x20 u16 x nodes: node offset / 4                      -> ctx+0x38 (node data follows: ctx+0x3C)
      node data ... (aligned 4)
      u16 x events: event offset / 4                    -> ctx+0x40
      event data ...                                    -> ctx+0x44
      u32 x (routers+1): R[k]; router k = u32 pairs from R[k-1]*4 to R[k]*4  -> ctx+0x48
      (R[last] = start of the track table)
      u32 x (tracks+1): T[t]; samples of track t = T[t]*4 .. T[t+1]*4        -> ctx+0x4C
      sample table: 8 bytes per sample, all tracks concatenated              -> ctx+0x50
      end of file = T[tracks]*4                                             -> ctx+0x54
```

**Node** (16 bytes + 4 per branch):

| off | field |
|---|---|
| +0 | s16 **sample, 1-based** global index into the sample table (0 or −1 = no audio). 0x3D43B0 reads `table[sample*8 − 4]`, the duration of sample−1, and 0x3D4400 reads `table[(sample−1)*8]`. |
| +2 | u16 flags: bit 0 track (0 = stream .mus, 1 = loop bank; the only values seen), bits 5–7 section (1..4), bits 12–15 = 0xF on control nodes, which have no audio (observed) |
| +4 | u32: bits 0–7 router id (1-based, 0 = none; 0x3D3C90). Bits 12–16 branch count. Bits 20–23 measures per segment. Bits 24–27 beats per measure. 0x3D4460 derives the ms per bar (`ctx+0x44 = ms / measures`) and per beat (`ctx+0x40 = ms / (measures·beats)`). |
| +8 | u32: bits 0–7 = 0x40 on audio nodes. Bits 8–9 sync mode: 0x100/0x200/0x300 are tested at 0x3D4440 and 0x3D3658 on the group head node. Bits 20–31 group head node, an index into the node table (0x3D4388). |
| +12 | u32: 0, 2 or 3 (unknown) |
| +16 | branches, 4 bytes each: `u8 lo, u8 hi, u16 target node` (0xFFFF = stop) |

Branch ranges partition 0..127: [0,0x7F) for a single successor, or thirds
(0–0x32, 0x32–0x64, 0x64–0x7F) and other splits for random variation. Songs have no Pathfinder
variables (`vars = 0`), so the selector is presumably random. That is an inference, not traced.

**Sample table entry:** `u32 value, u32 duration_ms`.

- Stream track: the stream starts at byte `value × 0x80` in the MUSDATA `.mus`.
- Loop-bank track: `value` is the first bank entry index. A segment is 4 consecutive 1-beat
  entries (408 cases) or one long ending entry (72 cases).

`duration_ms = floor(sampleCount·1000/rate)` holds for all 12329 samples of the 45 songs
(`validation.sampleDurationMismatches = 0`).

**Event:** a 16-byte head (`+0xC u16 id`, `+0xE u16 & 0x3F` action count) and 16-byte actions.
In an action, byte 0 is 1 or 2 (track?) and byte 9 is the opcode: 0x02, 0x04, 0x06 or 0x0A.
Bytes 12–13 are an s16 value: a node index for 0x04, a negative special for 0x04/0x0A, 3100 for
0x02, and 0x0100 with 3000 for 0x06. Bytes 14–15 are arguments. The exporter keeps each action
raw alongside these fields. Their semantics (start node, jump, fade, wait) belong to the
music-logic work and are not asserted here.

**What a song graph looks like**, using "Go" (135 BPM) as the example. It has 541 nodes, 479
samples (465 stream bars and 14 loop-bank phrases), 48 events and 5 sections. The graph works
like this:

- Audio nodes play one 4-beat bar each, at 1777 ms (`measures 1 × beats 4`).
- Linear chains follow the recording order, so consecutive stream segments join seamlessly.
- Three-way branch nodes at phrase boundaries pick the next variation.
- Control nodes (flags 0xF0xx, no sample) route between sections.
- Track-1 nodes (flag bit 0) play the "async" loop bank: 4-beat groups of 1-beat slices, plus
  two long endings chosen 0x45/0x3A (node 537).
- Across all songs: 18–651 nodes, 10–517 samples and 1–48 events per song. 38 songs have a loop
  bank.

### Music audio

Stream segments are EA-XA:

- Songs: 32000 Hz stereo.
- `mapsel` and most of `charsel`: 32000 Hz with **6 channels**.
- `Peak*Amb` streams: 44100 Hz stereo.

In the 6-channel segments, channels 0/2 and 1/3/4 are strongly correlated and channel 5 is often
silent, which suggests three stereo stems, 0/1, 2/3 and 4/5 (open question).
`toAudioBuffer(ctx, decoded, [[0,2,4],[1,3,5]])` folds them to stereo. Loop banks are EA-XA at
22050 Hz, mono or stereo.

## 9. Browser assets (`tools/export_audio.py`)

`web/public/assets/AUDIO/` holds about 1.35 GB. Every payload is the original compressed data:

- `catalog.json`: songs (every MUSIC.INF field), the playlist, the mixes, BANKS/CROWD/SPEECH.INF,
  and the lists of music, speech and banks.
- `music/<SongId>.json`: counts, table offsets, `nodes[]`, `events[]`, `routers[]` and
  `tracks[]` (file, stream/bank). `samples[]` gives `{index, track, kind, ms, offset/size` or
  `bankIndex/count, channels, sampleRate, sampleCount}`, alongside `inf` and `validation`. The
  original `.mus` files are copied unchanged.
- `speech/<name>.json`: the header plus `lines[] {index, offset, size, fields, channels,
  sampleRate, sampleCount, seconds}` and the original `.dat`.
- `banks/<name>.json`: the bank header and `entries[].patches[]` (codec, rate, loop, offsets,
  params) with the original `.bnk`. The `.eam` crowd MIDI files are copied as-is.

In `web/audio-decode.js`, `decodeSpeechLine(dat, line)`, `decodeMusicSample(mus, loops, sample)`
and `decodeBankPatch(bnk, patch)` each return `{sampleRate, channels, length, data:
Float32Array[]}`. `toAudioBuffer(ctx, decoded[, channelMap])` and `encodeWav(decoded)` convert
the result. The module has no DOM dependency, so it runs in workers and worklets.

## 10. Open questions

- The meaning of the Pathfinder event actions and of node `+12`. How the game picks events and
  branches (the music-logic work).
- The 6-channel layout of `mapsel` and `charsel`, and how the game mixes the stems.
- Tags 0x9C and 0x9D on music streams, and patch parameters 0x00–0x2A (envelopes, pitch; raw in
  the JSON).
- `.hdr` bytes +0/+1 (id or hash) and the exact random selection with its history.
- The `.eam` crowd MIDI (MIDx) format is not decoded.

## 11. Pathfinder field semantics (runtime, see audio-logic.md section 8)

- Node +2 flags: bits 0-4 track (voice index), bits 5-10 section (`3D4B00` reads >>21 of the word), bits 12-15
  loop count of an end node (1-7 = count down, 0xF/12/13 = endless).
- Node +4: bits 0-7 router, 12-16 branch count, **17-19 == 1: random branch value** ((now/23) & 0x7F, only on part
  heads), 20-23 measures, 24-27 beats per measure.
- Node +8: bits 0-7 0x40 on audio nodes, 8-9 sync mode of a part head (0x100 = start slices on the master's next
  beat, the only mode used), 10-14 event posted by a part head (0 everywhere), 20-31: for audio nodes the index of
  their part head (control node), for control nodes a running part id (not used by the runtime).
- Node +12 (w12): bit 1 = sliced (played one beat per master beat, bank entry value + slice - 1); bit 0 unused.
- Node sample: 0 = part head, -1 = part end (loop counter), -2 random, -3 post event (the last two unused).
- Branch lo/hi are signed bytes compared inclusively with the voice's intensity byte (not a random number).
- Event head +0xC: bits 16-21 action count, 22-27 runtime action index, bit 29 = disabled; +0 / +8 runtime times.
- Action: bytes 0-2 track mask (1 = stream, 2 = loops), byte 3 0xF1/0x1F (AND-ed with the slot id's high byte),
  +4 timeout (0), byte 8 runtime flags (bit 26 of the word = done), byte 9 opcode, +0xC operand (see audio-logic.md 8).
