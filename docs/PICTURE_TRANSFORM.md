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
axis, the implementation starts at the negative picture bound center, adds the
same picture's clamped extent for bit `1`, subtracts it for bit `2`, and applies
`floor(value + 1/8192)`. Bit `4` performs neither extent adjustment. A zero nibble
yields zero. These are the completed picture's own bounds, not parent/window
dimensions. Centers may be negative. Each arithmetic stage rounds to binary32;
floor consumes the double-promoted result. Nearest rounding and finite results
are required by the native implementation.

The bounds callback stores exactly two alignment components and invalidates the
picture cache after writing its center, extents and radius. Object translation
is not part of this offset and must not be added twice. Earlier descriptions of
these inputs as two sets of half extents were incorrect.

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
increment, stored XY alignment, perspective denominator, independent basis
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
eventual picture visitor unchanged. Original-data callers supply zero in the
generic alignment container's third component: the original alignment has no
Z field. Nonzero aligned Z is a native extension, not recovered behavior.
A zero picture width sets normalized X and its
resulting X basis scale to zero; a zero picture height does the same for Y.
Zero virtual-window Y scale and zero external Y basis scale are valid
multipliers. Invalid enums, non-finite inputs, negative picture extents,
non-positive viewports, and zero or non-finite values actually used as divisors
fail closed.

The recovered picture constructor initializes both picture-extent scale
components to exactly `1.0`. They are runtime object state, distinct from PRM
descriptor coordinate bounds. The portable startup path must therefore obtain
picture scale controls, object matrices, visitor values, and projection
inputs from explicit prepared runtime state or an explicit project policy; it
must not infer picture extents from descriptor minima or maxima.

## Startup viewport boundary

The separate [submission cache](PICTURE_SUBMISSION_CACHE.md) controls when these
equations are evaluated. Reusing its transform still visits every supplied
group; dependencies outside the submission position require explicit invalidation.

The recovered transform functions contain no hardcoded reference dimensions,
and the executable picture-cache path does not yet prove visitor values of 640
and 480. The portable startup consumer explicitly selects a logical 640×480
canvas as project policy. Mapping that canvas to physical output with centred,
uniform letterboxing is likewise portable output policy, not recovered retail
behavior. This boundary keeps the transform independent of Windows, macOS,
Linux, Steam Deck, display resolution, and output aspect ratio.

The original base-window path can leave the virtual-window scale components
used by picture preparation indeterminate. Separately, the concrete startup
chain produces no stable x87 value before the multiply represented by
`external_y_basis_scale`. Neither undefined state is treated as retail data.
Callers must provide explicit initialized virtual-window, external-Y-scale,
and `q` policies for that base-window path. The selected intro camera traversal
instead supplies the camera's retained normalized viewport and signed extents
of the pass rectangle; its service receiver is the camera, not the window.
This resolves `q` for that path, but not the undefined external Y operand.
The module does not invent `(1,1)`, attribute the external Y
operand to a renderer query, or supply any other fallback.

## Concrete intro draw-input join

`make_intro_picture_cache_input` connects the reviewed producers to cache
preparation. Its inputs are an explicit live hierarchy, picture and camera
endpoints, stored XY alignment, current picture scale, prepared camera services,
and an explicit native replacement for the undefined Y operand.

The hierarchy's owner endpoint is the camera resource, not its selected window.
The picture chain includes the synthesized scene root; the inverse camera chain
excludes that terminal root. Authored parentless resources therefore still need
their runtime parent. The synthesized root's constructor supplies engine identity
and zero position, but later changes must remain in the live graph.

The relative position goes directly to submission position. The relative basis
is transpose-combined with the picture's current local orientation before cache
preparation; that same local orientation remains a separate later cache input.
The adapter neither cancels those operations nor adds Center translation again.
Stored alignment becomes `(x, y, 0)`. Picture scale is not derived from textures
or descriptor bounds.

The transpose combination uses separately rounded binary32 products and sums
without fused multiply-add. This is an explicit portable arithmetic policy;
the surrounding hierarchy and cache helpers retain their documented policies.
No whole-pipeline x87 precision claim is made. Cache dependency invalidation
remains explicit, as described in [submission caching](PICTURE_SUBMISSION_CACHE.md).

Independent tests cover non-symmetric orientation, a translated terminal root,
signed rectangle overflow, scalar rounding, Center applied once, and the join
through ordered cache visitation into descriptor expansion. The private probe
also expands all real legal-picture groups using the owned camera, checked source
links, completed conditional Center state and decoded alignment. Its pass size
and external Y choice are explicit test inputs, not observed original values.
Neither test path establishes scene admission or renders the intro on screen.

Validation: all 54 local CTest executables pass. The new adapter test also passes
with GCC and targeted ASan/UBSan. These checks cover CPU state and geometry, not
pixel fidelity or a completed startup sequence.
