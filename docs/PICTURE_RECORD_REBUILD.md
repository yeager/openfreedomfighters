# Picture record rebuild boundary

`PictureRecordRebuild` is a disconnected bridge between an already accepted,
source-backed picture submission record and the existing ordered draw loop. It
does not load game data, decide visibility, allocate views, upload a texture,
submit GPU work, or present a frame.

## Entry and rebuild

The caller supplies a live record identity, owner-context identity, renderer
resource identity, checked static texture binding, and current submission
control. `accept` calls the required record-preparation service and then the
required backend-registration service before publishing the record to the
per-frame rebuild queue. A callback failure preserves its completed external
prefix, publishes no current record, and poisons the route.

At `rebuild`, the required live-view service resolves the record's current
owner-context association. An absent association is represented as no view and
uses order zero; it does not cause a camera or view to be created. The existing
key producer combines that current order with the real submission control and
selected image identifier. Queued records are stably merged with an already
sorted retained partition, including the documented new-before-retained
equal-key policy.

The result is `PictureOrderedDrawEntry` storage for
`PictureOrderedCoordinator` and `PictureOrderedDrawLoop`. Those components
still perform their own real preselection, view transition, binding, subtype
dispatch, and emit services. The output resource token is only a live runtime
association; it is not proof of texture availability or a completed draw.

## Limits

Record identities must be nonzero and unique across the retained and queued
sets. This prevents an ambiguous record-to-owner/view association in the native
model; it is a safety restriction, not a recovered source-data rule. Missing
hooks or identities reject before external callbacks. Failed rebuild validation
leaves the queued set intact; successful rebuild alone clears it.

This boundary requires prior device admission, state/view traversal, picture
acceptance, and later preselection/draw wiring to be provided by the host. It
must not be used to claim that normal intro startup has reached a visible
renderer frame.
