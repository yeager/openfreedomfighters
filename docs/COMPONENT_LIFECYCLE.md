# Retained intro component lifecycle

`IntroRuntime` now retains a component catalog alongside its existing hierarchy,
pictures, camera and controller. Each authored attachment keeps its full factory
name, owner handle, directory and attachment indices, identifier offset, owner's
deferred-reader offset and authored parameter. The synthesized `ZGROUP_RootGroup`
has a separate catalog entry. An owner-to-component collection joins these
records to the same owners used elsewhere in the host. This catalog collection
includes unconstructed and removed entries; it is not the live owner attachment
store required by runtime lookup and disposal.

The synthesized DefaultCam path now adds a concrete PreviewCamera instance,
retains its actual owner attachment and live-variable payload, and enrolls it
through the separate [ordinary manager](ORDINARY_COMPONENTS.md). The existing
authored catalog is not treated as a collection of completed factories.

A local run against the owned supported intro found 384 records: 383 authored
attachments plus RootGroup. Normal startup now constructs every supported
authored attachment with its recovered cold constructor state, while RootGroup
also receives its immediate initializer. This is not lifecycle completion:
the authored callbacks still require their real reader and runtime services.

`IntroRuntime::preflight_global_lifecycle()` derives its inventory from the
loaded scene and remains read-only. The complete supported fixture has 420
deferred reader identities, 383 authored attachment identities and 471 owner
identities including ROOT. Normal startup now enters the ordinary reader bracket
and reports ten reader identities covered by reviewed typed readers. It still
reports no component or owner coverage, and fails closed at reader coverage for
the remaining 410 identities. Global lifecycle admission is not attempted.

## Construction and ownership

Catalog insertion does not execute a factory. Concrete construction registers a
common-base identity before calling its supplied factory. The scene-manager-owned
`SceneComponentSequence` retains the next serial and live count; serials do not
reset when a source archive is loaded or a prior registry is destroyed. Exhaustion
is rejected rather than wrapping. The sequence must outlive its registries.

Common-base masks, status and script reference start at zero, with status `0x10`
added when construction mode is selected. The concrete factory can inspect that
state before returning its completed state and retained callbacks. It owes the
actual derived constructor, registered class defaults, owner wiring and event
enrollment. Authored parameters and owner-class ordinals are not substitutes.
An exception preserves registration and leaves the lifecycle failed.

Each common constructor also samples the live application dispatch clock and
uses the shared scene scheduling phase to set its initial clock and `0.1f`
interval. The phase advances by binary32 `0.1f`, wrapping to positive zero at
one. It is not reset for another component or registry. The explicit clock
supplier must remain valid and is called before serial assignment; sampling
failure leaves the phase/serial unchanged and fails the lifecycle.

The catalog and construction order are separate. The concrete RootGroup route
now registers its descriptor, binds/enrolls the component, marks immediate status
`0x2` and calls its real initializer. It does not mark global completion `0x4` or
run the global owner-notification tail. See [root resource state](RESOURCE_STATE.md).

## Global component passes

The supported stable-registry path runs every surviving constructed instance in
reverse construction order for phase one, then starts again for phase two. It
rejects unconstructed entries and foreign live components sharing the serial
owner: a partial registry cannot pass as whole-scene initialization.

For each entry, the requested phase bit is tested before the required loading
progress service. The service receives the cumulative surviving-node visit count,
including visited nodes without that phase bit. Loader offsets and denominator
remain caller-owned. The phase-bit test is not repeated after progress returns.

Attached owner, hide bypass, status and resource hide remain live inputs. There
is no script-reference owner fallback. A pure native owner lookup validates
handles; it is not an original event or a state-mutating callback. Missing owners
are skipped. Hidden owners block callbacks and retirement unless bypass `0x200`
is present. Phase one still notifies its captured owner after a hidden visit.

An eligible callback runs synchronously, then receives completion bit `0x4` or
`0x8`. Existing completion bits do not suppress later explicit global passes.
Missing callbacks fail at the named component and phase. MovieControl's phase-two
callback binds to the same retained host controller, application clock and sound
preferences; it cannot be used for another catalog record.

Retirement requires a concrete cleanup/removal service. It must preserve cleanup,
current-owner lookup, status and destruction ordering, including repeated cleanup
when the concrete route requires it. Only after success does the native registry
mark the record removed, decrement the live count and release its callback
captures. Stable metadata remains inspectable; later passes skip it. Phase-one
notification still uses the owner captured before retirement.

Callbacks may update live fields, but cannot reconstruct any registry sharing
the scene sequence or reenter
its lifecycle. Exceptions retain completed effects, mark the pass incomplete and
prevent retry. Stable tombstone storage and owner teardown are native safety
policies, not replicas of the original allocator.

## Remaining startup work

Normal startup constructs RootGroup and the reviewed authored directory, but
does not run global initialization or construct the deferred runtime state.
The remaining concrete constructors/readers/callbacks, live
owner flags, root/additional-owner loader hooks, progress behavior, retirement,
scene properties and shared command containers remain needed. The ordinary
dispatcher exists, but most admitted concrete callbacks are still missing.
The component-pass API does not perform those surrounding loader operations.

The intro sound owners are a real dependency: their initialization reads retained
sound records. A missing output backend does not justify empty sound callbacks.
Actual source binding and sound metadata must be established before that path
can run. No readiness event is manufactured to bypass it.

MovieControl now has one checked owner-reader boundary. It accepts only the
processed deferred record for the uniquely prepared controller, retains the raw
controller fields, and resolves the mandatory sequence/group list resources and
their ordered members in the live source-directory domain. It neither resolves
the remaining raw fields nor changes component status, lifecycle membership,
events, clocks, camera state, or rendering. Normal startup does not invoke this
single reader independently: the ordinary reader bracket remains all-or-fail
across every queued owner record.

The following component-reader boundary retains only the already constructed
MovieControl component identity, requested mask, priority, and declared events.
It requires the completed owner reader and does not set a live status bit,
enroll event 16, assign a deadline, or invoke phase two. Normal startup uses
this checked boundary; all other component-reader work remains deferred.

## Constructor-owned temporary

LensFlareControl uses a bounded nested common construction/destruction operation,
not general factory reentry. The temporary joins the real construction list and
consumes the shared serial, live count and scheduling step. It receives status
`0x20`, has no owner and performs no class notification or ordinary enrollment.

Cleanup unlinks it first, invokes an existing optional lookup-removal service
with its serial when its own status `0x10` is clear, then decrements live count.
The callback therefore observes an unlinked but still-counted temporary. Serial
and phase are not rewound. Retained lookup absence remains absence; construction
does not manufacture a lookup table. Native transient metadata is released after
successful cleanup and never enlarges the authored catalog. A service failure
preserves the completed prefix and poisons the lifecycle rather than retrying.

## Tests

Independent fixtures cover identity ordering, live counts, construction mode,
full reverse passes, progress ordering, hide/status changes, retirement, repeat
invocation and failure prefixes. The existing retained-intro fixture now enters
MovieControl phase two through this lifecycle and checks canonical clock/volume
changes and deadline assignment.

Other component callbacks and renderer services in that integration fixture are
synthetic. The fixture proves the connection, not complete original startup.
