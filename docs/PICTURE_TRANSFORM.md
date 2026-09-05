# Window-picture transforms

The renderer-neutral picture-transform module implements the recovered
window-picture alignment and cache-preparation arithmetic. It deliberately
starts after the virtual window service has produced its basis, scale, owner
projection scalar, and renderer Y scalar. It does not infer a world transform
or multiply the GMS construction chain.

## Alignment

The fourth required scalar in a serialized window-picture source is an enum in
the range 0 through 15. It maps to independent three-choice X and Y masks:

| Serialized values | Runtime masks |
| --- | --- |
| 0–8 | `11 12 14 21 22 24 41 42 44` |
| 9–14 | `01 02 04 10 20 40` |
| 15 | `00` |

The low nibble controls X and the high nibble controls Y. For each nonzero
axis, the implementation starts at the negative picture half extent, adds the
owner half extent for bit `1`, subtracts it for bit `2`, and applies
`floor(value + 1/8192)`. Bit `4` performs neither owner-extent adjustment. A
zero nibble yields zero. Public names intentionally describe the arithmetic
rather than assigning unproven left/right or top/bottom labels.

## Cached submission transform

`prepare_picture_cache_transform` accepts all values produced outside the
picture object explicitly. It reproduces the recovered operation order,
including the half-unit X/Y offset, viewport centring, fixed local depth
increment, aligned local position, perspective denominator, independent basis
scales, the engine's nine-float vector convention, object-matrix composition,
and final translation-Y inversion.

The engine convention for vector `v` and nine floats `m` is:

```text
out.x = v.x*m[6] + v.y*m[3] + v.z*m[0]
out.y = v.x*m[7] + v.y*m[4] + v.z*m[1]
out.z = v.x*m[8] + v.y*m[5] + v.z*m[2]
```

The same operation is applied independently to the three contiguous vectors
in a basis. Describing these bytes as a conventional row-major or column-major
matrix would lose the recovered ordering.

Picture descriptor local Z remains part of the neutral quad and must reach the
eventual picture visitor unchanged. Cache preparation also retains the input
and aligned-local Z values. A zero picture width sets normalized X and its
resulting X basis scale to zero; a zero picture height does the same for Y.
Zero virtual-window Y scale and zero renderer Y scalar are valid
multipliers. Invalid enums, non-finite inputs, negative picture extents,
non-positive viewports, and zero or non-finite values actually used as divisors
fail closed.

## Startup viewport boundary

The retail startup UI is authored in a logical 640×480 canvas, but the pure
transform function contains no hardcoded reference dimensions. The startup
consumer supplies the logical viewport and maps the completed result to the
physical output with centred, aspect-preserving letterboxing. This keeps the
retail transform independent of Windows, macOS, Linux, Steam Deck, display
resolution, and output aspect ratio.

The unresolved virtual-window hierarchy producer remains outside this module.
Construction-chain order is useful provenance but is not evidence that every
serialized GMS matrix is a parent-to-child render matrix.
