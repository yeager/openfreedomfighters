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

These facts establish identity and provenance. They do not establish a final
object-to-world matrix.

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

A corpus observation reinforces this boundary: under the simplest inverse of
the currently documented map quantization, only four of 2,801 descriptor
positions fall inside their associated spatial bounds. This disproves that
specific direct-coordinate interpretation; it does not identify the missing
coordinate conversion or the fields' final semantics.

The current opaque, blended, and fully transparent scheduling and diagnostic
depth policies are likewise renderer-development policies. They are not yet
claims about the original material or draw-order implementation.

## Evidence required for fidelity

World-space scene rendering remains gated on all of the following:

1. Trace the separate RMC and RMI descriptor-consumer paths for both observed
   object kinds and for primary and secondary handles.
2. Establish how GMS basis and position data reach a runtime object's render
   transform, including matrix layout, multiplication direction, handedness,
   units, and parent-relative behavior.
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
