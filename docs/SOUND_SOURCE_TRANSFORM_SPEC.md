# Sound preparation from canonical source transforms

This behavior contract connects sound preparation to existing mutable scene
state. It does not establish scene playback, global lifecycle completion, or
whether the stored coordinates are local-space or world-space.

## Identity and storage

Use the raw basis and position already stored in `IntroRuntime::hierarchy_`.
Each directory owner resolves to that storage through the canonical owner and
resource mapping. It is not the composed result of picture hierarchy traversal.
Keep parent metadata and class-data/primitive references separate from the
transform. Do not introduce a second mutable transform authority.

The source record contains nine binary32 basis values and three binary32
position values. Existing source allocation and directory initialization remain
the authority for their initial state. Expose snapshots by value or canonical
IDs; do not retain pointers into a vector that can grow when synthesized owners
are added. Missing, foreign, failed, or unassociated source mappings reject
without falling back to immutable parsed rows.

## Checked source update

Provide a source-driven update operation taking a live owner, a complete basis,
and a position. Validate canonical owner/resource identity and finite inputs
before mutation. Reuse the established directory transform-update behavior:

- Compare all three position values and the first six basis values bitwise.
- If those compared values match, do nothing, even if the last three basis
  values differ. Positive and negative zero differ for this comparison.
- Otherwise copy all three position values and all nine basis values into the
  same canonical raw state and set resource dirty bit `0x00100000`.
- Use the existing position-update service under its retained mode. The current
  supported directory mode has collection disabled and performs no backend
  callback. Reject unavailable modes/services before changing state; do not
  invent a renderer notification or a successful backend operation.
- Invalidate picture caches when the raw state changes, since picture rendering
  also consumes that state. Do not compose parent matrices, normalize the basis,
  or modify child transforms as part of this operation.

Factor the shared logic with `apply_directory_transform` so directory loading
and subsequent checked source updates cannot diverge. Existing direct pose APIs
may retain their documented semantics; they access the same raw state.

## Sound integration

`IntroRuntime::prepare_sound_owner` must obtain spatial values directly from the
owner's canonical live source state. Replace the caller-supplied spatial-data
callback; callers must not substitute a camera transform, picture-composed
transform, or a stale parsed-row snapshot. Keep the other existing explicit
resource, parent, and owner-enable services and their readiness checks.

Preserve current preparation ordering: validate the application clock, sound
bank and assigned source; establish the parent/time binding; honor resource
flag `0x400`; prepare the checked sound record. Only after successful record
preparation snapshot the live position and the first three basis values into
the audio record's position and direction. Do not normalize direction. Then
apply the existing explicit owner-enable predicate/callback and owner admission.
The owner-enable predicate is not a substitute for backend readiness.

A readiness skip/failure must not consume a spatial snapshot or fabricate a
prepared owner. An identity failure must not use another owner's state. Preserve
the runtime's existing failure/cleanup rules and resource effects. Once copied,
the audio record is a value snapshot: a later source update does not change it
until another supported preparation/update explicitly refreshes it.

The source state and owner live within the same runtime lifetime. No callback
should retain an unrelated process-global cache or a raw vector element pointer.
No global lifecycle admission is implied by creating this provider or by a
successful individual preparation.

## Independent validation

Use synthetic owners, source rows, and sound banks. Cover:

- Nonidentity basis and nonzero position initialized by normal construction.
- Successful sound preparation copies the raw first basis vector and position,
  even with a transformed parent, without picture hierarchy composition.
- Updating the source before preparation changes the resulting audio snapshot;
  updating it afterward does not retroactively change that snapshot.
- Canonical identity survives synthesized-owner insertion/vector growth; foreign
  owners, missing associations, and failed runtime state reject.
- Bitwise no-op update, signed-zero change, and an isolated change in the last
  three basis values; verify the documented comparison/copy distinction.
- Changed update preserves unrelated resource flags, marks dirty, uses the
  retained disabled-collection service, and invalidates cached picture output.
- Nonfinite input and unsupported update mode reject before mutation.
- Resource-disabled and backend-failed preparations retain their existing
  behavior; owner-enable false still differs from backend failure.

Update existing sound preparation tests to set actual raw source transforms
instead of injecting spatial answers. Keep the other preparation and failure
checks meaningful.
