# Scene-transform evidence boundary

This document separates decoded transform data from transform behavior that has
actually been established. The distinction is required for clean-room fidelity:
retaining a matrix or vector exactly does not establish how the original engine
uses it.

## Established data relationships

The portable data layer establishes the following relationships without copying
retail content into the project:

- each parsed GMS object source retains a finite nine-float basis and a finite
  three-float position;
- the reproduced GMS pool traversal assigns exact image-local runtime slots;
- direct geometry source classes resolve their packed primitive references to
  exact entries in the scene PRM catalog;
- each present RMC/RMI primary or secondary handle identifies a runtime slot,
  and locally populated slots resolve to exact GMS directory entries; and
- RMC/RMI spatial records retain their quantized bounds and their descriptor's
  finite orientation, position, auxiliary-position, and extent values.

Executable tracing further establishes that the GMS loader copies the source
basis into runtime matrix storage, copies the source position, and passes both
values to the newly allocated runtime object's transform setter before
registration. The loader processes each serialized row into a class-pool object,
stores that object's handle at the same row index, and uses the row's hierarchy
controls to select and attach it to its runtime parent. The generic attach path
writes the parent link later followed by renderer traversal. This establishes
the parsed GMS row, local transform, and authored parent chain as identities in
the constructed runtime hierarchy.

A separate renderer-path trace now establishes that a render batch resolves a
runtime object's local transform, walks its parent links until a root-marked
ancestor, and composes basis and position along that chain. The resolved batch
record also carries a separately transformed per-object vector and additional
object fields. The current trace also shows a separate owner base pose before
that parent walk and an object-local offset transformed by the resulting basis.
The relationship between parsed RMC/RMI entries, the renderer's geometry-entry
objects, and that owner pose has not been established. Nor has the renderer pose
record been joined to final backend submission and camera view/projection.
Consequently this evidence does not yet authorize composing
`SceneRenderInstance` fields or claiming a world-space scene result.

The traced view traversal obtains candidate handles from a component-owned
collection, resolves them, and applies component and object-flag filters. That
collection has not been identified as the RMC/RMI tables or as an exhaustive
enumeration of GMS rows. Eligible candidates retain their runtime object
identity when wrapped, but this does not prove that every map-resolved direct
PRM source reaches the view path. The view builder also writes intermediate
pose records to local scratch storage that is not passed to its next list
builder in the traced callsite. Treat those records as unconsumed there, not as
a verified draw packet.

The view-side geometry builders also derive frustum planes from view state and
test geometry bounds against those planes, updating visibility masks before
linking eligible geometry resources. This confirms that view state affects
visibility selection. The culling path does not write the three matrix pointers
later consumed by the traced BonesVS draw entry, so it does not establish that
the active camera is applied to submitted geometry.

A targeted trace of the BonesVS instance dispatch slot found resource
allocation, cache-lifecycle, and generic callback routes, but no call edge from
the per-view geometry list to that draw method. The draw method's matrix
triplet therefore remains unjoined to both the view-side cache entry and the
GMS runtime identity.

A view-state object also owns three pointer fields at the same offsets. The
BonesVS method reads its triplet from a record argument; matching offsets alone
do not establish whether that argument is the view-state object or a different
allocation. The caller's record identity and its relationship to active view
state remain unresolved.

These facts establish GMS runtime identity and hierarchy provenance. They do
not establish the complete submitted object-to-world or world-to-clip transform.

## Runtime scene-root selection

Bounded static tracing establishes that GMS construction obtains a
database-owned scene root and begins recursive object insertion beneath it.
The runtime camera's root accessor first attempts to resolve its optional
object-reference key; when that lookup has no result, it returns the same
database-owned root used by GMS construction. The camera base initializes this
key as empty, and the key can later be set or copied from another camera.

This establishes the shared root and fallback behavior, but not which camera is
active, whether normal scene initialization populates the optional key, or
whether that key resolves to a particular parsed GMS object. Therefore this
root relationship does not authorize a normal world-scene draw producer or
camera selection. Keep the normal path disconnected until scene geometry
entries join to the GMS runtime objects and the composed poses reach a verified
view/projection submission path.

## Source-only diagnostic convention

The current diagnostic renderer deliberately uses only the GMS source record.
For local PRM position `p`, source basis `B`, and source position `t`, it computes

```text
q[0] = B[0] * p[0] + B[1] * p[1] + B[2] * p[2] + t[0]
q[1] = B[3] * p[0] + B[4] * p[1] + B[5] * p[2] + t[1]
q[2] = B[6] * p[0] + B[7] * p[1] + B[8] * p[2] + t[2]
```

The basis is treated as three rows for this calculation. Intermediate results
are checked for finite, representable output. A single bounds-normalized
orthographic fit then maps all indexed diagnostic positions into clip space.
That fit, its selected view axes, and its depth order are presentation choices.

`q` is a **source diagnostic position**, not a recovered world position. The
formula does not read the RMC/RMI orientation, position, auxiliary position,
extents, or quantized bounds. Changing those map fields must not change the
diagnostic result. Code, logs, tests, screenshots, and user-facing status must
retain the word `diagnostic` when this convention is active.

Raw PRM local-space rendering is also safe as a diagnostic. Rendering every
directly materializable handle in stable map and primary/secondary order is a
useful inventory, but it is neither complete scene materialization nor evidence
of original placement.

## Unsupported interpretations

Until additional evidence is available, the renderer must not:

- multiply the map orientation and GMS basis in either order;
- transpose or invert either matrix to obtain a guessed placement;
- add, subtract, rotate, or otherwise compose the map and GMS positions;
- interpret the auxiliary position as a pivot, origin, target, or translation;
- scale geometry from map extents or quantized bounds;
- use either matrix to transform normals or choose winding and culling; or
- describe any such result as the original camera, world placement, hierarchy,
  or material behavior.

A controlled aggregate probe of 220 directly materialized instances found that
adding a speculative `0x8000` coordinate bias increased spatial-bound overlap
for one unproven transform candidate from zero instances to 197. That
correlation is not evidence for changing the query conversion: executable code
passes `0x8000` separately as the loose-octree root center, and the transform
candidate itself remains unproven. The result instead reinforces that corpus
overlap cannot establish world-transform composition on its own.

The current opaque, blended, and fully transparent scheduling and diagnostic
depth policies are likewise renderer-development policies. They are not yet
claims about the original material or draw-order implementation.

## Evidence required for fidelity

World-space scene rendering remains gated on all of the following:

1. Trace the separate RMC and RMI descriptor-consumer paths for both observed
   object kinds and for primary and secondary handles.
2. Map parsed scene records to the established runtime transform inputs and
   base pose, including matrix layout, multiplication direction, handedness,
   units, the separately transformed per-object vector, and parent-relative
   behavior.
3. Establish the meaning and coordinate space of map orientation, position,
   auxiliary position, and extents, and whether they replace, compose with, or
   only describe the referenced object.
4. Recover attachment, deferred, post-load, hierarchy, animation, and indirect
   source behavior needed by sources without a direct PRM reference.
5. Establish the exact relationship between dequantized spatial bounds and
   transformed render geometry.
6. Recover camera view/projection, depth, winding, culling, and normal-transform
   conventions independently of the diagnostic fit.
7. Validate a candidate formula across the supported corpus and against
   deterministic runtime observations, then add synthetic arithmetic tests and
   cross-backend image comparisons.

Candidate formulas must be evaluated rather than selected because they look
plausible in one preview. Unknown and external handles remain explicit; they are
never assigned placeholder transforms or geometry.

## Clean-room publication rule

Private executable listings, addresses, retail strings, retail identifiers, and
retail screenshots remain outside the public repository. Public work may contain
independently written parsers and renderer code, synthetic fixtures, behavioral
specifications, aggregate counts, and evidence conclusions that do not reproduce
protected game content. Every future fidelity claim should have a private
evidence record tying it to the supported executable build and an independently
reviewable public test.
