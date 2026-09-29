# Native shared race world

`engine/native_world.mm` now creates all six original Snow Jam participants from one data-only seed. It owns independent native riders, original animation players and recovered NPC provider state. `RiderWorld` owns the common race clock, path/checkpoint progress, ordered pair records and a single original random generator. The interactive client still uses its single-rider path while the shared runtime's missing air/grab and reaction lifecycles are completed.

The selected roster is updated in original phase order: all controllers, timers, filters, motion, local animation sampling, world pose, second motion/contact, geometry commit and events. Pair contacts run inside each actor's second motion stage after everyone's pose has been generated. Later actors see earlier impulses. Local sequence completion occurs after sampling and before world posing. The total game tick commits only after all actor events.

`tools/export_world_start.py` requires the exact EE hash used by the verified opponent assembly audit. It preserves the distinction between gameplay character attributes and visual model identity. The exporter writes profiles, initial physical/controller/animation state, authored AI paths, all six retained AI routes, relationship values and pair metadata. It excludes expected bone arrays, captured collision bodies, opaque NPC words and accepted-command recordings. Runtime code reads JSON and native assets; it does not read emulator memory or execute guest instructions.

Human body queries use the source kind1 detailed policy; NPC body queries use kind0. Ground/contact cache kind2 remains common. Course-progress permission uses the actual inverse of the source AC4 inhibition field. NPC pacing compares current course remaining distance with the first enabled human's progress. AI route updates use best remaining distance after course events. Human paths are visible to followed-peer logic but excluded from computer-rider traffic counts.

## Reproduce

```sh
.venv/bin/python tools/export_world_start.py local/reference/pcsx2/snow-jam-glide.p2s local/assets/native/ARA1/world-start.json
.venv/bin/cmake --build build/metal-engine --target ssx3_world_audit -j8
build/metal-engine/ssx3_world_audit local/assets/native/ARA1/world-start.json 1 local/native-qa/world-glide-1.json
.venv/bin/python tools/compare_world_reference.py local/native-qa/world-glide-1.json local/reference/pcsx2/snow-jam-glide-1.p2s local/native-qa/world-glide-1-comparison.json
```

The audit supplies neutral human controls and lets every NPC run its own native command provider. The runtime class accepts a human controller callback for eventual app integration. Unsupported paths stop with an error; the audit retains completed frames and records `completed:false`, the failed frame and reason. An unfinished frame is never reported as a completed simulation result.

## Current evidence and gaps

At the first original glide continuation, all six riders match exactly in position, velocity, physical quaternion, controller state, time scale, requested body animation, all60 collision sphere centers, course progress and AI route vectors/distances/heading. The NPC provider oracle independently verifies all five command words and provider state. `native_world_motion_pose_checkpoint` covers this specific one-frame scope and seed identity/output separation; it does not claim full gameplay equivalence.

Shared RNG still differs after that frame. Executing the complete original115D48 upper-body reaction routine identifies the two missing draws on slot4, Griff. They belong to that controller's timer/proximity branches; an unconditional two-draw workaround is not used. The native upper reaction lifecycle is being connected.

The shared runtime currently completes eight ticks, then stops at frame9 when Psymon enters control5 and needs a nonneutral airborne pose. Those eight ticks are execution evidence, not eight independently captured exact checkpoints. By the original30-frame endpoint Psymon and Moby also select grabs, requiring original grab choice, animation and angular gates. Soft/hard pair reactions and attack marker lifecycles explicitly report that their integration is pending. The crash motion kernels and NPC air provider are being recovered separately. Scenery callbacks, auxiliary translated geometry fields, scoring, full event initialization, UI and audio remain beyond this checkpoint.
