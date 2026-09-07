# Scene transition requests

`StartLoaderLoadScreenSource` is a narrow handoff from a caller-owned parser.
It accepts only the supported LoadScreen attachment identity, a finite zero
attachment parameter, an exactly consumed source wrapper, and the supported
target. It does not parse GMS or open an archive.

`LoadScreenTransition` models the component's ordinary-update boundary. An
explicit one-time setup service runs first; then an unsigned retained counter
advances. At a count of three or more it asks `SceneTransitionQueue` to clear
the current retained scene entries before queuing its copied target. The queue
only records deferred removal and target requests. It never loads a scene,
creates GPU resources, presents a frame, or accepts input.

`SceneTransitionPump` is the one explicit manager-owned consumer for this
verified route. It accepts exactly one retained `FF-Startup` target and delegates
the complete package preparation and source-backed construction transaction to
`StartupSceneLoader`; it has no separate resolver, preparer, or notification
handoff. This avoids consuming the pending request before the loader can use it.

The candidate package and factory-produced live scene are staged while the
deferred request, marked entries, and prior committed scene lease remain owned.
Only after both stages succeed does the manager retire marked entries, publish
the replacement scene, clear the retained target, and clear pending work. This
staging-before-retirement is an explicit portable safety policy: the observed
retail manager visits eligible removals before target selection. A failed or
throwing stage preserves the queue and prior scene state and does not retry.
The pump is nonreentrant; normal startup does not call it.

It has no generic target ordering/coalescing, archive search/opening policy,
rendering, presentation, input, or FF-StartUp menu construction. The source of
LoadScreen's initial counter and setup flag remains a separate construction
requirement.

## Checked FF-StartUp loading transaction

`StartupSceneLoader` is a disconnected, fail-closed replacement transaction for
the one retained `FF-Startup` request. Its caller supplies a callback which has
already resolved and completely prepared the checked owned archive, GMS source,
and SUP support input, plus a separate concrete factory that returns an explicit
live-scene token. The transaction owns opaque leases for all three inputs until
that scene is replaced.

`StartupScenePackageSource::prepare_checked` is the narrow source-backed
preparation helper for that callback. It accepts only `FF-Startup` and an
already chosen archive path, then opens the archive, requires exactly one named
GMS and SUP member, CRC-reads both, decodes/parses GMS, and parses the nonempty
SUP dependency list. Its three aliasing leases retain the archive, raw source
bytes, and parsed forms for the later factory call. It is not an archive search
policy, does not select companion resources, and does not construct a scene.

Any missing service, incomplete package, failed factory result, or exception
leaves the deferred request and previously committed scene unchanged. There is
no automatic retry. The manager-owned pump invokes this transaction and, only
after both callbacks have succeeded, retires marked entries before publishing the
new scene. It is nonreentrant and deliberately is not wired to normal startup.

It is not a generic archive parser or a reconstruction of the FF-StartUp GMS
factory. It does not initialize lifecycle callbacks, render, present, expose a
menu, or accept input.

`StartupBootSceneConstruction` is the next disconnected construction boundary.
It accepts a retained checked package and live scene lease, a caller-proven
complete directory mapping for the canonical ordinary-window source, the exact
`ZWINDOW_BootMenu` attachment with finite parameter `1`, a nonzero factory
generation, and live registry callbacks. Only a canonical factory-produced
window plus a live attached component produces its move-only
`StartupBootControllerToken`. The token keeps the package and scene lifetimes;
it cannot be copied into a global UI state or outlive the construction scope.

Construction is deliberately not component-reader completion, initialization,
input admission, focus, rendering, or scene selection. A missing registration,
allocation, canonical owner, exact attachment, or live component fails closed.
Normal startup does not call this boundary.

## Startup boot-menu admission

`StartupBootMenuAdmission` is a fail-closed boundary for a future
factory-produced FF-StartUp boot-menu controller. It requires caller-owned live
event-registry, window-coordinator, and action-map services. The caller supplies
only opaque runtime/action/routing identities; the boundary resolves and retains
the two event IDs, then performs one coordinator initialization.

It records delivery of the resolved typed action only. It does not parse a scene,
bind a keyboard/mouse/controller input, create a widget, decide focus or
selection, request another scene, or expose a retail menu. Normal startup does
not call it yet.
