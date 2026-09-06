# Picture preselection

`PicturePreselection` implements the ordinary ordered coordinator's selection
service over an explicit live owner-context registry. It appends record identities
through the coordinator's bounded visitor. It does not mark keys, set cursors,
activate owners, or infer registry membership from prepared pictures.

## Ordering

Inactive contexts are removed from this registry without skipping the next entry.
Active contexts resolve their backend owner, capture participation and override,
then query the selection interface, identifier and resolved selection object.
The override is read even for participation zero/one and is not reread after
callbacks. The view association is read after those queries. Consecutive equal
views share camera preparation within one invocation; the clear guard is untouched.

Camera-relative geometry is required even under override. A captured nonzero
override skips only the extension query and predicate. Otherwise the predicate
receives the computed point, current extension and previously resolved selection.
Passing contexts append their linked records, then collect the returned related
resources in order. Each related owner's resource enumeration retains its initial
receiver, independently of each resource's live owner. Duplicate paths remain
duplicate appends. Final key marking/restoration belongs to the coordinator.

## Native boundaries

The component owns registry membership only. Contexts, owners, views and record
chains remain caller-owned and must stay live. Callbacks must not destroy objects,
restructure chains, mutate registry membership, or run concurrently. Scalar and
association changes are observed only at their specified read points. Required
association resolvers must reject stale or mismatched objects; a missing optional
related owner instead skips its capability branch.

Identity tokens use nonzero integers; null optional identities use `nullopt`.
Selection identifiers and backend registry identifiers are scalar values and may
be zero. An initially null view skips camera preparation; changing from a nonnull
view to null rejects. Geometry must be finite. Related queries receive exactly
eight output slots and must not write beyond them. Counts above eight reject
before use. Each record/resource walk rejects cycles and an explicit positive
step bound. Each invocation permits at most 8192 append calls, including duplicates;
the coordinator visitor additionally enforces its total list capacity.

All required hooks are checked before entry, including for an empty registry.
Entered failures poison the component and preserve earlier removals and appends.
There is no rollback or implicit retry. A caller must rebuild state after failure.
Reentry is rejected before effects; a propagated rejection poisons the outer call.

## Remaining integration work

Actual owner activation must populate this registry using its backend extension
query. `register_context` appends between runs, preserving duplicates, only after
the caller establishes membership. Insertion during a run or after poison rejects
before mutation. Source picture/camera parsers do not establish extension availability.
Owner-relative geometry and selection/related-resource services remain explicit
dependencies, not fabricated successes. An empty test registry does not establish
that FF-Intro has no selected owners or special rendering work.

The bounded synthetic tests exercise callback ordering, captured override, live
view lookup, inactive removal, duplicate paths, related-owner enumeration, capacity,
cycles, missing contexts, finite geometry and failure prefixes. They do not claim
an original-game runtime capture.
