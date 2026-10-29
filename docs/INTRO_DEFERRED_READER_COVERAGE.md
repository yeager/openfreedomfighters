# Deferred-reader implementation boundary

This page no longer publishes source-derived reader inventories, payload shapes, or population counts. Detailed analysis stays in private research storage.

The runtime should admit a reader only when its owner identity, bounded input, and output state are validated. Reader order and failure behavior must be covered by independently authored synthetic cases. Unsupported forms must remain unavailable rather than being inferred from a similar component.

The controller parser can validate an explicitly bounded block copy and locate
its reference fields. A successful parse does not prove the copy's origin,
owner, reference-service generation, or relocation state. The normal reader
now translates supported controller references in an owned copy using a
scene-owned project registry and resolves them through its live lease. These
tokens are project identities; they do not establish native handle identity or
complete the list, camera, and frame paths needed for intro playback.

Supported list blocks are likewise checked as bounded copies, translated
through the scene registry, and compared with the controller's prepared member
lists. This verifies project-side consistency; it does not establish that the
original member reader or its helper has run.

The supported first-cut form has its own bounded block parser because its
attachment data follows the reference prefix. Its reader validates the full
form and resolves the sequence reference through the scene lease. This is a
project-side consistency check; it does not establish camera activation.
The supported sequence form is also checked against an owned bounded block;
its candidate references resolve through the same live scene lease before
reader state is published. This does not determine which candidate is selected.

For the supported sequence attachment, the reader also retains a bounded,
scene-owned attachment stream. Its scoped first-phase callback reads that copy
and registers a shared project timing identity before completing. A terminal
reader failure retires the stream. The timing identity also carries a
scene-owned event-generation lease: clearing the event table invalidates it,
even if a later declaration reuses the same numeric identity. This protects the
scoped callback's lookup; it does not implement a deferred-start queue, native
event delivery, global lifecycle completion, or intro playback.
