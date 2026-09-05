# Descriptor picture expansion

This CPU-only operation implements the recovered D3D8 descriptor-emitter
geometry contract. The observed picture resource subtype selects an instance
factory whose callback reaches this emitter, and its resource mode selects the
40-byte descriptor branch. This does not establish the final startup frame's
renderer selection, draw-list acceptance, projection, or compositing state.
The operation is not connected to the native GPU runtime yet.

## Inputs and geometry

Consume the authored center and full spans preserved by `PictureQuad`, not
values reconstructed from its rounded bounds. The supplied
`PictureCacheTransform` retains its nine basis components without reordering
and its translation separately. Group consecutive basis components into
vectors B0, B1, B2. For authored center (x,y,z), the expanded center is:

```text
C = x*B2 + y*B1 + z*B0 + translation
H = horizontal_span * 0.5 * length(B2)
V = vertical_span   * 0.5 * length(B1)
```

The linear center is materialized as binary32 before translation is added.
Expansion produces these vertices, all at C.z:

| Vertex | Position XY | Authored UV |
| --- | --- | --- |
| 0 | C.x-H, C.y-V | u_min, v_max |
| 1 | C.x+H, C.y-V | u_max, v_max |
| 2 | C.x+H, C.y+V | u_max, v_min |
| 3 | C.x-H, C.y+V | u_min, v_min |

UV endpoint names do not imply sorting or clamping. Reversed endpoint pairs
remain reversed. Rotation and shear affect the center and basis lengths;
the quad remains axis-aligned in this emitter's XY plane. Transforming each
local corner separately would produce a different result.

Triangles use (0,1,3) and (1,2,3). Every byte of the packed ARGB modulation
word is independently floor-divided by two, including alpha, then copied to
all four vertices. This vertex-stage reduction does not prove half brightness
or opacity in the final image. Conditional downstream operations are described
in [Picture material requests](PICTURE_MATERIAL_STATE.md); their final startup
inputs remain unresolved.

## Portable policy and limits

The portable operation accepts at most 4,096 descriptors, preserving their
order in batches of at most 2,048. Output indices are batch-local, restarting
at zero; each batch identifies its first descriptor in the input. These owning
CPU containers and the overall input cap are project policies, not claims
about original memory ownership or maximum scene size.

Non-finite inputs/results, negative spans, and unrepresentable output floats
are rejected. These are supported-data safety policies, not recovered retail
diagnostics. Numerical evaluation uses double intermediates with explicit
binary32 materialization boundaries; it does not claim bit-exact reproduction
of mixed original extended-precision arithmetic. The existing undefined
virtual-window and external-Y inputs still require explicit caller policy.

## Integration gates

The public transform layout agrees with the reversed-vector equation in
[Picture transforms](PICTURE_TRANSFORM.md); it must not be transposed into a
conventional XYZ layout. A caller must still supply a justified transform.
The cache producer has already composed the object matrix and inverted the
translation Y component; expansion must not repeat either operation.
This function does not derive runtime owner extents or projection inputs.
The separate [picture projection boundary](PICTURE_PROJECTION.md) consumes
explicit resolved projection inputs after expansion, with identity WORLD/VIEW.
It supplies no missing startup camera defaults.
Final frame renderer selection, draw-list scheduling, depth/culling/clipping,
sampler state, texture-stage and blend behavior, and physical-output mapping
must be resolved before presenting this as faithful startup rendering.

## Validation

Public tests use authored fixtures for corner/UV order, channel reduction,
nontrivial bases, batching, precision boundaries and invalid inputs. The local
startup asset compatibility test additionally expands every prepared retail
descriptor with an explicitly test-only identity transform. That check proves
the real descriptor data can cross this CPU boundary; it does not validate
retail placement or final pixels, and writes no retail-derived output.

## Owning startup expansion boundary

`StartupGraphicsExpandedPlan` joins a prepared startup plan to explicitly
provided picture transforms. Transform records are keyed by the exact picture
directory identity, not their position in the caller's array. There must be
exactly one transform for each prepared picture; missing, duplicated and
unmatched identities are errors. No fallback transform is generated.

The result owns resource metadata, picture metadata and expanded geometry.
It preserves requested/effective state, immediate submission order, picture
identity and each submission's resource identity. Each source submission is
expanded independently, without merging across textures or reordering draws.
The already validated 21-picture/77-submission and 7-picture/7-submission shapes
bound this operation. No pixels, borrowed source pointers or GPU handles enter
the result.

Opaque picture controls are preserved, not interpreted as blending inputs.
In particular, authored alpha is not silently combined with the descriptor's
packed color. This boundary makes an explicitly transformed CPU plan available
to a future renderer, but does not supply original runtime transforms or
justify the still-missing final render state.
Each prepared quad and expanded submission also owns the original texture
resource record and its PRM-relative identity. These remain per-group even
when image identity is shared, and are not treated as final runtime material.
