# Ordinary grab scoring

The ordinary1352A8 path resolves a grab index through150118. The15 mappings
come from45AEB8 + index*20 +4; score definitions come from530600 + scoreID*8.
The first signed word is the begin-point value and the second is the hold-point
value. `reference_grab_score.py` exports those authored values and the initial
state under rider790. It never exports a future score outcome.

## Begin and end

119708 adds `beginPoints * float0x38D1B717` to accumulated14. It stores
`holdPoints * float0x35DFB23B` in holdIncrement3C. A negative holdSeconds40 is
started at0; a nonnegative timer is preserved. IDs35 and above set activeUber5C.
1176F8 writes−1 to comboTimeoutA4, suspending the grounded combo-expiry timer.
It does not clear the accumulated hold threshold index or completed durations.

1197D8 first runs119068's three-entry history insertion. It appends into the
first empty entry unless the immediately preceding entry already has the ID;
a full history is retained. IDs below19 increment normalCount4C,19..34 increment
tweakCount50, and35+ increment uberCount54. For35+, rider boost tier2F4 >=6 also
increments superUberCount58. Counters use original signed-word wrap behavior.
The end call adds holdSeconds40 into totalSeconds44, updates longestSeconds48,
clears activeUber5C and sets holdSeconds40 to−1. It preserves the increment and
combo timeout fields.

Both119708 and1197D8 return **exact positive zero**. Their following10E098 calls
therefore do not modify boost, even though the caller supplies a boost-category
mask. This is verified against the complete original10E098 function and an
unchanged full rider object. Awarding boost immediately on begin/end would be
incorrect.

## Held ticks and authored bonuses

The117D24..117D70 branch of117C28 runs during121818 after1125C0 AI route progress.
When holdSeconds40 >=0, it advances by `timeScale * float0x3C888889`. In contrast,
accumulated14 adds holdIncrement3C exactly once per tick without time scaling.
A zero time scale would stop duration growth but not this score increment if
the scoring logic itself still runs.

119210 checks the single current authored threshold, not every crossed entry:

| Hold seconds | Bonus points before multiplier |
| --- | --- |
|3|4000|
|6|2000|
|9|1000|
|−1|Sentinel; no further bonuses|

On a crossing, it advances holdThresholdIndex8C once, computes the bonus as
`trunc(points * multiplier1C4 + .5)`, and adds it to bonusPoints84. The original
notification sequence clears event types28,29,30,31,32, then requests event32
with duration1.5, the scaled points, and the integer threshold seconds. The
native helper returns that typed event; it does not invent UI or audio effects.

117948 computes current pending trick points from
`multiplier1C4 * accumulated14 *10000 +5`, truncates to signed integer, then
removes its signed remainder modulo10. BonusPoints84 remains a separate field.

## Scope and validation

`engine/grab_score.hpp/.cpp` implements the exact begin/end state, held branch,
threshold request and pending-point getter. The score object is shared with
spins, flips, airtime and other activities. accumulated14 must eventually be
updated by those source handlers too; it must not be mistaken for a separate
complete grab-only scoring subsystem. Combo commit, final total awards and
boost conversion are later lifecycle work (including11A228 and117718), rather
than implied by these zero-returning begin/end calls.

`tools/test_grab_score_native.py` passes20,000 cases per begin, end, held tick
and point getter, including3,142 authored bonus events. It executes the original
119708,1197D8,119068,1176F8,150xxx,119210,117948 and10E098. The held prefix is a
label-bounded original117C28 block. Notification calls1179E0/117B88 form an
explicit typed event boundary; their original arguments and order are checked.
EE scalar guard-bit addition and original rounding are preserved throughout.
