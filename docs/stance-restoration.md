# Original stance restoration (115640)

`engine/stance_restore.hpp/.cpp` implements the complete control-independent
115640 transition. It is used when a temporary prewind/side stance ends, including
soft-control completion. It does not choose a new control or motion mode.

| Input | Physical state | Animation actions |
| --- | --- | --- |
| Style328 = 0 | Exact no-op | None |
| Motion1, style3 | Unchanged | Reset default root; request282 |
| Motion1, style4 | Unchanged | Reset default root; request277 |
| Motion1, other nonzero style | Unchanged | Request268 |
| Motion0, style3 | Rotate around old physical up by +π/2 | Transform existing roots; reset default root; request5 |
| Motion0, style4 | Rotate around old physical up by −π/2 | Transform existing roots; reset default root; request5 |
| Motion0, other nonzero style | Unchanged | Request5 |
| Other motion, nonzero style | Unchanged | None |

For every nonzero style,328 clears only after the animation request. The final
116930 call is an exact no-op in this executable. The source does not modify
reverse stance320, mirror flags, ground-contact forward/lateral vectors,
prewind control triplets or velocity.

Physical rotation uses the original11DFE0 arithmetic followed by its11E098
normalization/basis rebuild. The rotation axis is the *old* physical1C0, not the
terrain normal. The full angle constants are float0x3FC90FDB and0xBFC90FDB.

311B48 transforms all existing sequences across six channels with a Z-axis
quaternion generated from `sincos(-angle * .5)`. It does not update the default
root. The subsequent direct default-root stores set position to zero and the
quaternion produced by `sincos(-0)`, preserving the original signed-zero details.
Mirror flags on both the animator and sequences remain unchanged. Animation
requests pass blend−1 and flags0 to3128E8.

The optional callbacks run in source order: publish physical orientation,
transform existing sequence roots, reset the default root, request the clip.
They observe the old style; the helper clears it afterward. Without callbacks,
the result supplies typed effects for an existing deferred animation-event
pipeline. Callers must preserve their ordering and avoid treating a default-root
reset as a request to change mirror flags.

Validation: `tools/test_stance_restore_native.py` executes original115640,
11DFE0,11E098,311B48,311BF0,314760,31BE50 and116930. It compares native physical
quaternion/bases, every existing sequence root, default root, style and animation
request arguments. The3128E8 call is an explicit animation-request boundary;
clip creation/fading is handled by the separate native animation lifecycle.
