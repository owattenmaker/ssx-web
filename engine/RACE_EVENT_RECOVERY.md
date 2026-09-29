# Snow Jam event, race clock and course progress

The native helpers in `race_event.cpp` use original data and recovered source
operations. They do not invent a finish box, race duration, path distance, or
checkpoint location. They are an event-system component; native menus, start-gate
presentation, opponent behavior, scoring/results screens and persistence are
separate integrations.

## Authoritative data

The live race world is `*( *(gp-0x848)+0x84)`, also stored at `gp-0x488`.
Its `+0x0C` game-info object is `0x005BC500` in the Snow Jam captures. Construction
at `0x22F010` calls `0x1286A0`; its interface marker is `0x458488` at `+0xCC`.

`reference_race_event.py` extracts the clock, all participant start transforms,
finish/progress fields, path caches, checkpoint masks, and runtime course paths.
It validates pointer bounds and the known interfaces and fingerprints the course
and source EE image. `race_event_assets.py` separately reads the owned AIP
resource using the pinned SSX-Library layout reference.

Snow Jam is ARA1: PS2 SSB kind 14, track 8, resource 0, in assembled chunk 33.
Its 63,148-byte AIP contains 129 AI paths and eight track paths. Every track-path
origin and every segment float4 matches the live runtime bytes exactly. The raw
segment contains horizontal direction X/Y, vertical change per horizontal unit Z,
and horizontal segment length W. Source paths begin at `0x005C2800`, stride 60;
the global path manager is `0x004D33A0`, count `+0x10`, array `+0x14`.

The human starts on path 3 with **353496.15625 cm remaining**. The explicit course
events are:

| Path | Local horizontal distance (cm) | Runtime event | Raw AIP event |
|---|---:|---|---|
| 4 | 33742.61328125 | 11, value 0: checkpoint | 18, value 0 |
| 5 | 45287.2578125 | 11, value 1: checkpoint | 18, value 1 |
| 7 | 19361.896484375 | 1, value 0: finish | 0, value 0 |

These are point intervals in the authored path event stream, not independent
spatial trigger boxes. The raw-to-runtime IDs above are verified against both
owned AIP and live data; other event IDs remain uninterpreted by the extractor.
The manager permits three checkpoint indices in this event; only two explicit
checkpoint events appear in ARA1's track paths. Finish/result bookkeeping must
not be inferred solely from that count.

## Race clock and start ordering

`0x113B48` provides original names and handler selection:

| State | Name | Enter/update behavior recovered |
|---:|---|---|
| 1 | GameInit | Update selects Freeride |
| 2 | Freeride | Entry clears race-enabled `+0x14`; update selects Race |
| 3 | PreRace | Entry clears race ticks and handler-local `+0xB8` |
| 4 | Countdown | Entry sets `+0x1C` to 180; update decrements positive count, otherwise selects Race |
| 5 | Race | Increments race ticks; requests EndRace/results when all human riders are finished |
| 6 | EndRace | External finish notification; clock does not continue race ticks |
| 7 | Shutdown | Requests external cleanup |

`0x113B10` changes the selected state while retaining the previous handler.
`0x113C20` executes its exit and the next entry on the following update, then runs
the currently selected handler. A transition during that update remains pending
until the next update. Countdown exit clears its count. Its 180 constant is
literal source code at `0x113D38`; the native test covers the old-count-zero
boundary and deferred Race entry notification explicitly.

Game-info `+8` is total simulation ticks, incremented at the end of `0x128AF0`
(`0x129134..0x129144`); `+0x0C` is race ticks incremented by `0x113DB0` before the
rider update. The native clock helper returns external notification requests.
Network handlers and cleanup are not silently implemented as complete systems.

The ready snapshot has state 3, previous 0, total/race ticks zero. Glide has
state 5 and total/race ticks 338/158. Snapshot phase and native UI/start ordering
still need a measured countdown anchor before claiming matching launch timing.
Original UI entry `0x234AD0` selects Countdown for ordinary race modes;
`0x234C68` dispatches `StartgateOpen` when the countdown field is nonpositive.

## Course projection and events

`originalRacePathProject` implements complete `0x26A638`. It uses the original
horizontal metric, segment clamping, closest-point tie behavior, and cached
forward search. The cache contains a segment start position, accumulated local
distance, and segment index; its search horizon extends 3000 cm from the cached
start. A valid cached interior projection can terminate early. Scalar arithmetic
uses the EE alignment guard and scalar square root policy; vector operations use
VU chop.

`originalRaceSelectPath` implements `0x1127F0`, including the three-candidate
bounding-box ordering from `0x26B178`, 796 cm velocity lookahead, authored path
sampling, and the current-path cache reset threshold of 1592 cm. It compares the
original combined geometric metric, rather than choosing an arbitrary next path.

`originalRacePathEvents` implements the `0x26A090` remaining-to-local-distance
conversion and `0x26AA80` inclusive interval query with the rider's twelve-event
capacity. Progress `rider+0x4D0` is current remaining distance; `+0x4D4` is the
best remaining distance reached. Reversing therefore does not move the event
progress boundary backward. `originalRaceProgressStep` implements the surrounding
`0x112FB0/0x112338` update and path-selection rules for the Snow Jam course; the
separate global bonus threshold array is zero in these fixtures.

## Checkpoints and finishing

The event callback `0x10E5D8` sends runtime event 11 to `0x270AB0`. It resolves
the human roster index, checks the original manager inhibition fields, validates
the checkpoint index, and sets the human's `+0x5FC` bit only once. A corresponding
bit in the manager's notification byte `*(manager+0x3B4)+0x1F` requests downstream
processing. The native helper preserves duplicate suppression and MIPS shift
behavior.

Runtime event 1 reaches `0x125108` for eligible race modes when the rider has not
already finished. Rider `+0x470` is the elapsed-after-finish marker/timer: negative
means unfinished, zero begins the finish. It is incremented by the original
1/60-second scalar operation in `0x12102C..0x12104C`. `+0x478` stores finish ticks
as game-info race ticks plus rider penalty ticks `+0x47C`.

`0x12A250` returns true only when every configured human rider has a nonnegative
finish marker (and true for an empty configured list). AI completion is not a
condition in this predicate. Rendering, scoring, rank/payout/progression and
finish-animation control changes are external to the recovered arithmetic.

## Verification

`tools/test_race_event_native.py` uses opcode-corrected development copies of the
owned executable, with the unchanged original instructions in the measured
stages. Network/results/cleanup callbacks are explicit test boundaries.

- 20,000 race state/clock updates: exact integer state.
- 20,000 cached course projections: exact output floats and cache state.
- 20,000 checkpoint-mask updates: exact masks/notification byte.
- 20,000 course-event interval queries: exact records and order.
- 10,000 complete original Snow Jam path selections: exact selected path/cache.
- 2,000 complete original Snow Jam progress/event stages: exact selected path,
  current/best remaining distance, cache and emitted event records.

The native CTest covers countdown and deferred-entry boundaries, finish/results
transition requests, duplicate checkpoints, reverse progress, finish crossing,
penalty addition and floating-point mode restoration. Full native start-to-finish
playability additionally requires the caller integrations described above.

`race_event_asset.h/.mm` loads the extracted course and participant state through
Foundation into native typed structures. The separate native asset CTest walks
the authored route with controlled kinematic positions: it records exactly two
checkpoints, one finish and the EndRace/results transition. This validates the
event component and loading, without claiming an original-physics or AI race run.
