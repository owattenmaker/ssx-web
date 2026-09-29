# Air-adjust animation driver11

The original dispatch1036A0 uses456950[11] to call1043F8. The driver handles
semantics297..304 with two animation slots. It does not free-run the primary
pose clip.

## Exact clip mapping

These are basic-bank AFL indices; native packed IDs are `index <<8`.

| Semantic | Primary AFL index | Secondary AFL index | Direction suffix |
| --- | --- | --- | --- |
|297|135|143|L|
|298|141|149|FWDL|
|299|138|146|FWD|
|300|142|150|FWDR|
|301|136|144|R|
|302|140|148|BWDR|
|303|137|145|BWD|
|304|139|147|BWDL|

Primary clip names are `AADJ_INTO_ROT_<suffix>`; secondary names are
`AADJ_ROT_<suffix>_CYC`. Generic104CF8/311710 semantic initialization resolves
primary leaves[157,163,160,164,158,162,159,161]. Driver1043F8's own secondary
leaves are[165,171,168,172,166,170,167,169]. It reads those packed IDs directly
from the loaded lookup and consumes no RNG for the secondary slot.

Every definition297..304 has class1, kind11, completionKind0, channel2,
blendSeconds float0.23000000417232513, firstFadeIn0 and endFadeOut0. All16 source
clips have empty AFL event arrays. Export these definitions explicitly: a
filter limited to driver kinds0/1/2 omits them. Existing `initialClip` and
`followupClip` fields can carry primary and secondary IDs without another
asset-loader schema.

## Initialization and each update

Generic311F00→314518 initializes one enabled nonlooping primary slot, time0,
slot rate1 and slot weight1. No implicit completion63 event is installed for
kind11 with completionKind0. Keep ordinary sequence fade/root/mirror setup.
The first1043F8 update creates/enables secondary slot1; do not advance an extra
secondary frame during sequence creation.

1043F8 reassigns slot1's mapped clip and duration every tick through313C50.
When slot1 was disabled, that setter initializes time0, rate1, weight1 and
enabled1. An already-enabled slot preserves its time and rate, including when
its assigned clip changes. The driver then sets slot1 looping.

Let `m=clamp(max(abs(rider28C),abs(rider298)),0,1)`, using the **filtered current**
values after1211F8. The driver sets:

- Secondary weight = `.75*m + .25` with original scalar addition.
- Primary weight = `1-secondaryWeight`.
- Primary time = `m * primaryDuration`.
- Sequence seekPending = true.

Only slot1 advances using3135B0, with dt=`timeScale * float1/60` and the ordinary
slot-rate × sequence-rate order. It wraps using the original loop arithmetic.
Because it is slot1, it does not alter the sequence's completion, marker or
raised-event flags. The primary is sought directly; it must not pass through
ordinary primary-clock/event processing afterward.

Finally the driver advances the sequence fade once using the same dt. A
completed removal fade requests3145F8. The native helper returns that request
for its owning sequence container to remove the sequence.

## Native API and verification

`originalAnimationAirAdjustStep` is in animation_sequence.hpp/.cpp. Supply the
sequence, mapped secondary packed ID/duration, filtered28C/298 and timeScale.
The helper allocates the second native slot when absent, preserves original
state, and returns the removal request. `originalAirAdjustSecondaryLeaf`
exposes the verified secondary mapping (source fallback165 outside297..304).
Call this branch before generic slot advancement in the native player.

`tools/test_air_adjust_driver_native.py` passes30,000 complete original1043F8
cases using the actual313C50 clip initialization,313CF0 seeking,3135B0 looping,
and313800 fade functions. It checks both slots' IDs/times/durations/rates/
weights/flags, sequence completion/marker preservation, seekPending and removal.
The run includes10,000 secondary-slot initializations and801 removals. Container
removal is an explicit callback boundary; no original runtime code ships.
