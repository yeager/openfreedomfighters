# Retained intro component lifecycle

`IntroRuntime` now retains a component catalog alongside its existing hierarchy,
pictures, camera and controller. Each authored attachment keeps its full factory
name, owner handle, directory and attachment indices, identifier offset, owner's
deferred-reader offset and authored parameter. The synthesized `ZGROUP_RootGroup`
has a separate catalog entry. An owner-to-component collection joins these
records to the same owners used elsewhere in the host. This catalog collection
includes unconstructed and removed entries; it is not the live owner attachment
store required by runtime lookup and disposal.

A local run against the owned supported intro found 384 records: 383 authored
attachments plus RootGroup. Every authored identifier matched the public source
reader. All remained unconstructed, with no assigned runtime identity. This
checks the real catalog, not original runtime admission.

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

The catalog and construction order are separate. RootGroup's immediate phase-one
call, source-reader preparation and other factory effects must be implemented by
the concrete route. RootGroup must not be marked phase-one complete merely because
the original loader invokes it directly before the global pass.

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

Normal startup builds the catalog but does not construct or globally initialize
its real component population. Concrete constructors/readers/callbacks, live
owner flags, root/additional-owner loader hooks, progress behavior, retirement,
scene properties, shared command containers and ordinary dispatch remain needed.
The component-pass API does not perform those surrounding loader operations.

The intro sound owners are a real dependency: their initialization reads retained
sound records. A missing output backend does not justify empty sound callbacks.
Actual source binding and sound metadata must be established before that path
can run. No readiness event is manufactured to bypass it.

## Tests

Independent fixtures cover identity ordering, live counts, construction mode,
full reverse passes, progress ordering, hide/status changes, retirement, repeat
invocation and failure prefixes. The existing retained-intro fixture now enters
MovieControl phase two through this lifecycle and checks canonical clock/volume
changes and deadline assignment.

Other component callbacks and renderer services in that integration fixture are
synthetic. The fixture proves the connection, not complete original startup.
