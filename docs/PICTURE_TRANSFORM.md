# Window-picture transforms

The renderer-neutral picture-transform module implements the recovered
window-picture alignment, hierarchy production, and cache-preparation
arithmetic. It does not treat every serialized GMS construction link as a
conventional scene matrix.

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

## Hierarchy producer

`produce_picture_hierarchy_transform` accepts an explicit bounded node array
and picture/owner indices. Each node contains the recovered nine-float matrix,
position, and parent index. It starts the basis with the recovered vectors
`(0,0,1)`, `(0,1,0)`, `(1,0,0)`, then applies the picture node and each parent
in forward order. Position follows the same chain, adding each node's local
position after its matrix operation.

Owner-relative adjustment follows the owner chain to its terminal root, skips
that terminal node, and unwinds from the deepest nonterminal node to the owner.
For each node it subtracts local position and applies the transpose-form
operation:

```text
out.x = v.x*m[6] + v.y*m[7] + v.z*m[8]
out.y = v.x*m[3] + v.y*m[4] + v.z*m[5]
out.z = v.x*m[0] + v.y*m[1] + v.z*m[2]
```

The same operation is applied independently to all three basis vectors. The
producer rejects empty or out-of-range endpoints, invalid parents, cycles,
non-finite values, and picture/owner chains ending at different roots.

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

The recovered transform functions contain no hardcoded reference dimensions,
and the executable picture-cache path does not yet prove visitor values of 640
and 480. The portable startup consumer explicitly selects a logical 640×480
canvas as project policy. Mapping that canvas to physical output with centred,
uniform letterboxing is likewise portable output policy, not recovered retail
behavior. This boundary keeps the transform independent of Windows, macOS,
Linux, Steam Deck, display resolution, and output aspect ratio.

The original base-window path can leave the virtual-window scale components
used by picture preparation indeterminate. This undefined stack state is not
treated as retail data. Callers must provide an explicit, initialized
virtual-window policy; this module neither invents `(1,1)` nor supplies any
other fallback.
