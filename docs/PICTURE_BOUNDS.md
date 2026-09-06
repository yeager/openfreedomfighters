# Descriptor-derived picture bounds

`graphics::compute_picture_bounds` calculates local bounds from materialized PRM
descriptors and their actual draw groups. It is a pure geometry operation, not
the complete materialization callback or position-triggered ancestor update.
The surrounding local application is described below; ancestor owner updates
remain separate integration work.

## Geometry and numerical boundaries

Groups are visited in storage order, each span in ascending descriptor order.
Overlapping spans and duplicate visits are preserved; unreferenced descriptors
do not contribute. Finite sentinel minima/maxima and strict comparisons are
retained instead of initializing the union from the first descriptor.

Each visited descriptor contributes its XY center plus/minus half its edge span.
Descriptor Z, texture coordinates and modulation color are not inputs to this
bounds calculation. The union is scaled by explicit picture width/height controls;
its center Y is negated. The planar Z contribution is zero.

The result keeps both raw extents and resource-clamped extents. Each resource
extent has minimum `2^-13`. Radius is computed from the raw extents, before this
clamp, with the reviewed rounded squared-sum, square root, narrowing and `+1`
sequence. Clamped Z must not be substituted into that radius calculation.

Arithmetic preserves the reviewed binary32 operation boundaries and requires
nearest rounding. Square root uses `std::sqrt` on the double-promoted binary32
sum, then narrows to binary32 before adding one. This is an explicit portable
numerical policy, not an unqualified cross-platform bit-exact claim about the
original square-root implementation.

## Ordered materialized application

`PictureBoundsApplication` now implements the local callback around this geometry.
It always calls the explicit renderer-resource query using the live resource
identity, distinct renderer identifier and actual writable center/extent storage.
The query is not replaced with a guessed failure or an authored PRM-key lookup.

On query success, the returned bounds are retained without an extent clamp, and
their raw squared extents determine the base radius. On failure, the original
branch clears radius and center and sets the minimum extents, overwriting any
partial query output. These are evidenced branch effects, not assumed initial
state. Both paths preserve the required runtime dirty-flag writes.

The descriptor-derived center, extents and radius then replace the base bounds
in their reviewed order. Extent clamping and runtime flag updates remain distinct;
radius uses the pre-clamp extents. No ancestor update is inserted between base
initialization and descriptor replacement. This callback does not invalidate the
picture transform cache, mark component status, notify the position service or
mirror ancestor-owner bounds; those are separate caller operations.

As a native safety policy, descriptor geometry is prevalidated before invoking
the query, whose inputs and descriptor storage must remain stable. Missing hooks
or unsupported descriptor inputs reject without effects. Unexpected query
exceptions or invalid successful-query arithmetic retain the query's live writes
and poison the application; further calls reject. No rollback or original retry
behavior is invented. False query output is overwritten even when nonfinite.

## Admission and remaining state work

The native supported subset requires bounded groups, at least one visited
descriptor, finite consumed centers/spans, nonnegative spans and positive finite
scales. Signed negative spans are rejected as a native restriction, not claimed
as original validation. Ignored descriptor fields do not receive invented bounds
semantics. Nonfinite intermediate results reject instead of producing invalid
scene geometry.

The private owned-data probe computes the legal picture's bounds from its complete
PRM group table with explicitly supplied constructor scale and verifies enclosure
of the real grouped XY endpoints, the Y convention and planar extent clamp.
No retail descriptors or expected retail bounds are published as public fixtures.

Current ancestor bounds must retain other children; the root cannot be rebuilt
from only the legal picture. Source hierarchy and identity bases are useful
provenance, but do not prove unchanged runtime transforms, bounds, flags or
suppression. No zero ancestor initialization or automatic propagation is inferred
by this helper. The actual renderer-query outcome and original materialization
caller scheduling remain unproved; both query branches are tested explicitly with
the real owned descriptors. Actual first-frame admission remains incomplete until
the remaining state transitions are connected.

All 49 local CTest executables pass after this addition. The geometry and
application tests also pass with GCC and targeted ASan/UBSan, and the private
owned-resource probe validates both explicit query outcomes and real enclosure.
