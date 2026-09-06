# Picture submission cache

The CPU submission cache models the recovered distinction between preparing a
picture transform and visiting its groups. It does not select startup camera,
visitor parameters, final material state or GPU scheduling.

`PictureSubmissionCache` starts dirty without a usable transform. A submission
provides an optional ordered table of `BoundPictureDrawGroup`, explicit
`PictureCacheTransformInput`, an opaque uint32 control and a synchronous visitor.
An absent table returns without consuming inputs or modifying cache state.
A present table with zero groups still participates in transform preparation.

For a present table, all three submission-position components must be finite.
Numerical component equality is the cache comparison, so positive and negative
zero compare equal. A dirty cache or unequal position invokes the existing
[transform preparation](PICTURE_TRANSFORM.md). A clean matching position reuses
the prepared transform. Other preparation dependencies are not additional keys:
in particular, `cached_basis` in the input is an upstream basis, not the previous
computed cache result. Non-position inputs are not consumed on a clean hit.

Every successful submission visits every supplied group in order, including
groups with no descriptors. It passes the current paired texture binding,
descriptor sequence and current control unchanged. A transform cache hit does
not suppress these visits or merge groups sharing an image. Whether an empty
group yields a render record belongs to the downstream visitor.

## Invalidation boundary

`invalidate()` marks the cache dirty without discarding its previous values.
Completed loading/restoration and changed picture alignment, basis or position
are established original invalidation events. The original basis setter uses
word comparison, whereas the position comparison is numerical. This class does
not own those setters or reimplement their comparisons; their callers must
deliver the corresponding invalidation events.

Changes to viewport dimensions, owner state and other preparation dependencies
must also be connected to an explicit invalidation policy before runtime use.
The original submission routine does not itself compare these dependencies,
and the full resize/hierarchy propagation route remains unverified. No implicit
resize key, object defaults or inherited Y-basis value is invented here.

## Portable failure and callback policy

Non-finite consumed positions are rejected. Transform computation uses the
existing finite-input and arithmetic checks. A failed recomputation keeps the
previous position and transform but leaves the cache dirty. Successful
preparation commits position and transform together before invoking visitors.
This rollback policy is portable behavior, not a claim about the original
non-throwing numeric path.

Visitors receive a local transform snapshot for the duration of submission.
They may invalidate the cache for the next call, but recursive present-table
submission to the same cache is rejected; an absent table remains a no-op.
The identity-bound object cannot be copied or moved. A visitor exception propagates after any preceding
visits; it does not roll back those effects or a successfully prepared cache.
An empty visitor is an error for a present nonempty table, before mutation.
Absent or empty tables need no callable visitor. Callbacks must not mutate or
invalidate the borrowed group storage, and this object is not thread-safe.

These explicit callback policies are not recovered original exception or
reentrancy semantics. Tests of this boundary establish ordered CPU behavior,
not first-frame placement, presentation or original pixels.
The optional local compatibility test also passes the real startup groups
through both cache preparation and reuse into descriptor expansion. Its
transform parameters are explicitly test-only, not measured startup values;
the test writes no retail-derived output.
