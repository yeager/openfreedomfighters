# Scene directory construction

Status: local whole-directory construction is implemented. This boundary does
not establish scene activation, intro playback, renderer admission, or
compatibility with an owned installation.

## Input and ownership

After ROOT and source-load initialization, the runtime walks the parsed local
directory once. Each row's parsed pool group and class partition determine
resource allocation and handler selection. Its parsed parent reference
determines attachment to an earlier canonical owner; an absent parent attaches
to ROOT. Group construction and entry into a child pool remain separate
properties.

Each row receives one canonical owner/resource mapping and any supported
attachments and deferred-reader work. Unknown classes, unsupported records,
invalid parents, and failed construction stop the walk and leave the runtime
failed with only its completed prefix published. Directory positions select
records; they do not select class behavior or scene roles.

Controller, camera, window, and cut roles are resolved from parsed typed
records and local references. Missing or ambiguous required bindings fail
explicitly. Construction completion requires every directory row to have a
live mapping. It does not run the deferred readers or admit component updates.

## Acceptance checks

Use independently authored synthetic scenes with different directory sizes,
parent relationships, pool partitions, and component positions. Verify that
resource mappings match source records, parents resolve to canonical owners,
and deferred-reader work preserves source identity and order. Include malformed
parents, unsupported classes, missing registrations, and ambiguous typed roles.
Confirm that a failure preserves only the completed construction prefix and
does not enable camera registration, component phases, presentation, or
playback.

## Current verification limits

The synthetic tests cover directory mapping, parent ownership, reader ordering,
typed external-command selection, and fade-target reader completeness. A
source-backed normal session also covers the reader bracket, MovieControl's
deadline wait and event-16 activation, and both first-cut player initialization
phases. It stops at lifecycle receiver closure; it does not cover the
scene-frame adapter, full Fade-session integration, or playback. The latest
TRV2 run passed all nine intro test groups and the GMS image tests.

A private owned-installation readiness run completed directory construction
and the reader bracket. It still reports incomplete reader coverage and does
not admit lifecycle execution, renderer creation, audio, or playback. This
validates the loading boundary, not end-to-end game compatibility.

Intro playback and end-to-end scene activation remain unimplemented. Passing
the directory-construction checks does not establish that the intro plays.
