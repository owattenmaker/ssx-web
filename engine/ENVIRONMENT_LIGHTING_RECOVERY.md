# Original environment colour

`2ED490` updates each rider's environment record at `4FA370 + slot*F0`.
ARGB at record+28 colours board trails and snow; record+38 is the corresponding
lighting ratio. The source snow context doubles RGB; board trails retain the
original colour. Neither effect should substitute white for blue roof lighting.

The native `environment_lighting` helper recovers CPU texture sampling3889F0,
terrain colour2EDB20, filtering2ED1D0/2ED338 and the ground/air colour branches of
2ED490. Scalar ADD/SUB use the recovered EE guard-bit policy; scalar DIV uses
nearest, other arithmetic uses chop. No console runtime is linked to these
product files.

2EDB20 samples original PS2 base UV and lightmap UV. The lightmap's raw alpha
is doubled to form strength. Per RGB channel the target is
`(base-light)*globalTint*(lightAlpha*2)`, with tint(.55,.535,.55); ratio is
target/base. Alpha becomes.5. Ground filtering retains approximately.1 of the
previous value and.9 of the new target. The shared force-next flag makes the
next eligible ground update immediate. Missing resident patch data fades toward
white; rails retain colour and set the flag. Air mode uses live trajectory+788
status, predicted patch/time/UV and elapsed time to blend toward airborne white
or toward the predicted landing patch. It reads the current trajectory, not the
pre-control cache used by animation selection.

`environment_asset` loads `environment-lighting.json`. `export_environment_lighting.py`
retains PS2 data independently of the GameCube atlases used by course rendering.
All source chunks of each exported course are resident: source patch+156 names
a streaming chunk, **not a material RID**. Original eligibility checks track
state6 and chunk state3. Those runtime streaming states are represented by the
native complete-course residency assumption; dynamic chunk streaming is not
implemented here.

| Course | Terrain patches | CPU textures |
|---|---:|---:|
| A |291|10|
| ARA1 |1913|74|
| BRA2 |1787|55|
| CRA3 |2272|81|
| DRA4 |2183|90|
| ERA5 |1739|71|

Texture lattices retain raw source RGBA, including alpha0..128. Indexed4 and
indexed8 CPU layouts follow the original swizzle and palette permutation.
Original3889F0 clamps sample indices inclusively to width/height. Fetches beyond
authored allocation data remain marked unknown; a nonzero-weight access raises
`OriginalEnvironmentUnavailable`. The effects caller may retain its last valid
colour and report that limitation. Corrupt package metadata uses ordinary fatal
errors instead. No guessed guard pixels or wrapped edge values are introduced.

Validation:
- `python3 tools/test_environment_native.py`:20,000 full original3889F0 raw-RGBA
  samples,20,000 full2EDB20 material/UV cases and20,0002ED338 filters bit exact.
- `python3 tools/test_environment_live.py`: initial glide filter state plus
  current glide+1 terrain UV and exported authored texture data reproduces all
  four live glide+1 ambient float values exactly.
- Both start textures (base65 indexed4, lightmap144 rawRGBA, live handle932)
  match all16,384 interior cached original texels each. This confirms the source
  package's pixels and alpha are the actual CPU sample data.
- Objective-C loader syntax and independent native executable pass.

`reference_environment_lighting.extract` exports only current filter state and
static global values; it does not export future colours. ARA1 riding-start was
seeded only after its EE SHA256 matched the original glide snapshot. Main/effects
integration belongs to the parent agent. The complete ground/air dispatcher has
source inspection coverage; the randomized oracles cover its sampling/filter
callees, and the live joined check covers the grounded first frame. Later air
colour histories are not yet claimed bit exact.
