# Scene transition requests

`StartLoaderLoadScreenSource` is a narrow handoff from a caller-owned parser.
The installation verifier now confirms the supported `StartLoader.GMS` has one
bounded LoadScreen source and preserves its authored target. This remains a
discovery boundary: normal startup does not yet instantiate `LoadScreenTransition`
or enqueue the target.
It accepts only the supported LoadScreen attachment identity, a finite zero
attachment parameter, an exactly consumed source wrapper, and the supported
target. When admitted from the typed GMS parser it also retains the source
directory ordinal through the deferred transition for provenance; this is not
a runtime identity. It does not parse GMS or open an archive.

`LoadScreenTransition` models the component's ordinary-update boundary. An
explicit one-time setup service runs first; then an unsigned retained counter
advances. At a count of three or more it asks `SceneTransitionQueue` to clear
the current retained scene entries before queuing its copied target. The queue
only records deferred removal and target requests. It never loads a scene,
creates GPU resources, presents a frame, or accepts input.
A missing or throwing setup service leaves the counter, setup flag, and queue
unchanged.

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

`StartLoaderPreparedRoute` is a separate, production-independent handoff for
testing the proven path without admitting it to normal startup. It receives a
caller-owned checked `GmsImage`, derives its source only through the bounded
StartLoader parser, requires the explicit reviewed initial state (counter zero
with setup pending), and permits only a caller-selected canonical
`FF-StartUp.ZIP` path. Its ordinary updates perform the retained LoadScreen
request: setup once, then package preparation on update three. It returns a
checked `StartupSceneLoadPackage`. After preparation it can derive a
`StartupBootSceneDirectorySource` only from that package's exact typed GMS
input, preserving the checked chain from StartLoader to the BootMenu source.
This remains source evidence, not a scene factory, manager-state mutation,
lifecycle, renderer, input, or presentation path.

`StartupBootSceneDirectorySource` is the corresponding source-only boundary
inside the prepared `FF-StartUp.GMS`. It accepts one exact ordinary-window
owner with the reviewed `ZWINDOW_BootMenu` attachment and parameter, and
retains that owner's closed hierarchy subtree for the existing construction
boundary. It does not allocate a window, make a scene live, resolve BootMenu
event identities, choose an active root, or render a menu.

`StartupBootSceneFactory` is a disconnected join between those two checked
inputs and the existing live-construction boundary. It accepts only a package
with typed factory inputs and a BootMenu directory source derived from that
same retained GMS object, then delegates allocation to caller-supplied live
services. It neither supplies those services nor starts a transition, scene
lifecycle, input path, or renderer.

## Checked FF-StartUp loading transaction

`StartupSceneLoader` is a disconnected, fail-closed replacement transaction for
the one retained `FF-Startup` request. Its caller supplies a callback which has
already resolved and completely prepared the checked owned archive, GMS source,
and SUP support input, plus a separate concrete factory that returns an explicit
live-scene token. The transaction owns opaque leases for all three inputs until
that scene is replaced.

`StartupScenePackageSource::prepare_checked` is the narrow source-backed
preparation helper for that callback. It accepts only `FF-Startup` and an
already chosen archive path, then requires the exact twelve-member
`FF-StartUp` package family: `ZGF`, `SUP`, `BUF`, `GMS`, `TEX`, `SND`, `LOC`,
`OCT`, `SGP`, `RMC`, `RMI`, and `PRM`. It CRC-reads every member, decodes/parses
ZGF and GMS, parses the nonempty SUP dependency list, and validates the GMS
references against its paired BUF. The remaining named resource bytes are kept
opaque under a package lease for a later factory. It is not an archive search
policy, does not interpret those companion formats, and does not construct a
scene.

After that validation, the source-backed package also offers an immutable
`StartupSceneFactoryInputs` value for a future factory: parsed ZGF, parsed GMS,
and the paired BUF bytes. Its typed leases retain all three inputs after the
package object itself is released. The generic `StartupSceneLoadPackage::complete`
test API does not manufacture that value. The input is data only: it neither
chooses an owner nor invokes a reader, factory, lifecycle callback, renderer,
or menu.

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

Private static review further establishes that `CBootMenu` obtains component
storage through the shared live object-allocation service, rejects a failed
allocation, runs common window/component construction, and then installs its
concrete method table. The table has distinct reader and one-time initializer
entries; they must not be merged. The backing pool, allocation size, ownership
destruction route, and reproducible allocator identity have not been
recovered. Portable code must therefore keep allocation as an opaque
caller-provided live service rather than derive identities from source order,
pointers, or an assumed allocation sequence.

## Startup hierarchy construction snapshot

`StartupWindowHierarchyFactory` is a separate, disconnected construction-link
boundary for a future factory-built FF-StartUp tree. It receives a complete
parsed source scope, a scene lease and factory generation, then reads only
factory-proven runtime nodes while an explicit hierarchy read guard is held.
It validates that every retained source child has one canonical runtime owner
and that the live sibling chain matches the recovered attachment policy:
container-family children in reverse source order, followed by leaf-family
children in source order. Its move-only snapshot is bound to the lease,
factory generation and a guarded hierarchy epoch; it invalidates on a changed
binding.

The snapshot contains only source/runtime parent and intrinsic child/sibling
links plus factory-derived family metadata. Its preorder helper is a pure
construction traversal. It does not select an active root, evaluate hide or
component filters, establish visibility, choose a view, construct transforms,
or submit/present a draw. Normal startup does not call this boundary.

`StartupActiveWindowRootProvider` is the distinct proof boundary for that
missing selection. It accepts only a coordinator/manager-returned live root,
then checks it remains a member of the complete factory hierarchy with the
same generation and epoch. It retains the manager's opaque scene-lease and
pass-context identities; it never derives either one from a directory index,
BootMenu component, graphics subtree, source order, or host viewport. A root
token still does not admit a camera/view or renderer pass, and normal startup
does not call it.

## Startup boot-menu admission

`StartupBootMenuAdmission` is a fail-closed boundary for a future
factory-produced FF-StartUp boot-menu controller. It consumes the move-only
construction token in two stages: component reading resolves/stores its first
opaque registry result before the common reader; initialization runs the common
window step, resolves a second opaque result, routes the retained object, and
only then sets its completion latch. Both lookup keys and results are runtime
registry identities, not serialized action IDs.

The reviewed reader consumes no reached BootMenu-local serialized scalar,
string, reference, or platform-key value: it resolves and stores its first
opaque 16-bit registry identity, then invokes the common reader. The separate
initializer runs common initialization, resolves its second opaque identity,
routes the retained object/window with that result, and only then latches
completion. Equality of the two identities is not established. The field at
the concrete component's `+0x180` offset is a name/key field after concrete
construction, not a secondary method table.

It does not deliver actions, parse a scene, bind keyboard/mouse/controller
input, create a widget, decide focus or selection, request another scene, or
expose a retail menu. Normal startup does not call it yet.
