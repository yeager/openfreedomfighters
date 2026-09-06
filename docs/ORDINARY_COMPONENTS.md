# Ordinary component updates

`OrdinaryComponentManager` owns pending additions and retained membership. It is
separate from the reverse-construction global initializer. New membership keeps
the admitted mask and registration cache distinct: enqueue does not set the
cache; successful merge insertion does.

Refresh removes stale handles and cached entries whose ordinary admission was
cleared. Entries with both bits already clear remain in the collection but are
skipped during dispatch. Pending entries are sorted by priority, then each
equal-priority run by the original truncated class/serial key. Equal-key swaps
and shared generator advancement are retained; this is not a stable sort or a
single lexicographic sort. The generator belongs to `ApplicationServices`, so
another scene/consumer continues its current state rather than reseeding.

Before callbacks, dispatch refreshes membership, publishes the scene integer
clock to the application dispatch alias, and captures pause/filter state. It
checks live admission for each entry. Callback additions wait for the next
refresh; admission changes affect later entries in the current pass. Removal
notifications defer compaction during traversal. Native event 16 and the nonzero
script-reference owner route remain separate services. The native wrapper checks
phase-one completion and preserves the post-callback owner/retirement order.

Native limits are 600 pending and 1,200 retained entries. Idle pending overflow
refreshes before appending. Traversal overflow, exhausted retained capacity,
priority `0xffffffff`, reentry and unsupported retained-key changes fail
explicitly. Completed effects are not rolled back. Profiling is optional and
requires real hooks when enabled; unimplemented concrete callbacks must throw.

`IntroRuntime` creates this manager only when actual ordinary admission first
requires it. A hidden, unadmitted PreviewCamera does not create an empty manager
or publish a dispatch-clock alias. Queries do not allocate it.

`IntroRuntime` uses this manager for its actual RootGroup and synthesized
PreviewCamera components. Native handles derive from the common constructor's
serial, not a GMS index or camera ID. The Preview factory preserves construction-mode
status, attaches its live-variable payload, applies the visible-owner enrollment
rules, and appends real ordinary membership. The concrete Preview global handlers
have no effects; this does not license empty handlers for other component types.
The host requires the complete global initializer before an ordinary pass.

RootGroup has a real repeated global initializer, but its ordinary input-action
processor remains unimplemented and fails by name when reached. Creating its
pending entry does not make ordinary scene execution complete.

Tests cover partition collision vectors and generator state, two-stage sorting,
duplicate caches, stale removals, shared state across managers, pause/filter and
phase gates, callback mutations, owner routing, retirement and capacity failures.
Scene integration tests supply explicit synthetic factories for other objects;
they do not establish that the retail intro's full component population runs.
Normal SDL input/frame dispatch and most retail component callbacks remain to
be connected. Prior users of the shared sorting generator must also retain their
effects when those paths are implemented.
