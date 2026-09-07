# Startup BootMenu boundary

`StartupBootMenuAdmission` models a narrow, two-stage lifecycle boundary for a
factory-produced FF-StartUp BootMenu component. It consumes the move-only
source-backed construction token, so neither stage can outlive its FF-StartUp
scene transaction.

`read_component()` first verifies the live owner/component and registry,
resolves and retains one opaque registry identity, then invokes the common
window-component reader. It produces a move-only reader-complete token.
`initialize_component()` consumes that token, runs common window
initialization, resolves a second opaque registry identity, routes the retained
object with that second result, and only then reports `initialized()`.

This is not input admission. The first resolved identity is opaque; it is not a
key, controller button, action-map entry, or proof that the native component
handler accepts it. The two registry keys/results remain separate opaque roles;
this boundary neither observes nor dispatches input. It does not create focus,
select a menu item, request a scene, or make a retail menu interactive.

The source-backed construction token remains a separate earlier boundary. It
proves the ordinary-window owner and exact `ZWINDOW_BootMenu` attachment exist
in one scene transaction; it does not complete the owner reader, component
reader, common initialization, or retained routing step.

Private clean-room tracing has established that the LinkMenu state-one route
first invokes a named `PressStart` action service, then releases the resolved
action entry before it directly invokes LinkMenu's `STARTGAME` operation. The
action service lookup/release path has no recovered subscriber fanout, callback,
menu owner, scene-manager request, or archive loader. Public code must therefore
not bind a key, controller button, F10 action, or `PressStart` directly to a
guessed level. The direct LinkMenu owner route remains a separate boundary.

That owner route is now partially recovered too: its reached virtual operation
can conditionally look up the retained `rGameMenu` object and changes internal
owner state before delegating to common window handling. It still has no
verified level identity, player/mission operation, or scene-manager request.
`rGameMenu` is therefore an intermediate live-object boundary, not permission
to infer a first campaign scene from a menu label or archive ordering.

A further private trace shows that the reached route retains an opaque target,
changes its internal menu state from 2 to 4, and triggers a related runtime
transition. Adjacent dispatchers likewise pass only opaque retained targets;
the direct consumers observed so far are menu/window synchronization and event
paths. No scene-manager request, archive path, mission, player creation, or
single-player target occurs in this recovered segment. It remains a menu-state
transition only. The next evidence target is the `rGameMenu` factory and its
target-array population, followed by the eventual selection action.

That factory investigation corrects an earlier assumption: `rGameMenu` is a
named runtime-object lookup, not a registered class or executable factory.
`BootMenu`, `LinkMenu`, and `LevelMenuSinglePlayer` are distinct registered
factories. The recovered `STARTGAME` handler belongs to `LinkMenu`; its source
reader carries six opaque object identifiers which preparation
resolves/filters/binds without making them scene targets. The owned startup
data contains a separate single-player menu root with a selectable `C01A`
child, but static evidence does not connect that object to `rGameMenu` or to a
scene request. The native scene-manager request pair remains unreached, so no
selection is wired in the portable runtime.

The first concrete selection boundary is now recovered without assigning a
mission to it: a recognized menu command selects an index, resolves one of the
retained runtime targets, and passes that object to an owning menu/window
virtual receiver. A paired action target is toggled for the corresponding
selection. The target and receiver types remain runtime/data-dependent. This
path contains no recovered scene-manager clear/request, archive, scene,
mission, or loader identity. The next trace follows the verified receiver; the
portable runtime must not equate the selected object with a campaign level.

The next receiver-binding trace establishes that the LinkMenu field which leads
to the dynamic owner interface is not populated by the LinkMenu factory,
constructor, source reader, or preparation path. Construction initializes it
empty and teardown clears it; the factory receives no owner argument. The
binding must therefore be made by generic component attachment or a later
generic post-creation route. No owner vtable, final receiver, scene request,
level, or mission edge has yet been recovered. The next trace targets that
generic attachment boundary rather than serialized LinkMenu data.

That generic boundary is now recovered: a shared inherited LinkMenu virtual
setter replaces a prior non-null attachment through the old receiver, then
stores the supplied owner in both LinkMenu owner fields. It is only attachment
state; it issues no scene request. Multiple generic call sites exist, but the
source-load/post-creation caller that attaches this LinkMenu and the concrete
owner vtable remain unresolved. The receiver chain must therefore remain
disconnected while tracing those callers.

The startup source model distinguishes two identically labelled objects. The
menu-controller attachment belongs to the window object; the similarly named
picture has no such attachment. The recovered generic attachment path still
stops before a concrete selection receiver or scene transition, so no
game-start behavior is inferred.

The recovered GameMenu owner is an ordinary window. Its selection receiver
first resolves either an optional nested owner or the scene's window root. In
the recovered fallback path, the window root replaces retained active-window
state, emits paired window-state updates, and calls a configuration/input
helper. It contains no recovered scene-manager clear or request, archive,
mission, or gameplay construction. The optional nested-owner route and the
subscribers to the state updates remain untraced, so this is a fallback-path
boundary rather than proof that all selection processing stops there.

The optional nested-owner route is now resolved for this startup window. Its
authored reference points directly to the same startup window root reached by
the fallback lookup, so it bypasses the name lookup but converges on the same
active-window state update. It likewise establishes no direct scene-manager
request, archive, mission, or gameplay construction. Any later effect must be
traced from the emitted state updates rather than inferred from this route.

The direct recipient of the active-window update remains under trace. An
earlier vtable attribution was rejected during verification and is not used as
evidence here. The verified root state update alone is not a scene-manager
request or proof of a level launch; any downstream transition must be recovered
from the correctly identified recipient rather than inferred from menu state.

The corrected root path can filter data-backed reference/value records, resolve
the selected reference, and invoke common callbacks on the resolved object. The
startup root's deferred source does not populate that record list; its ordinary
references are not dispatch records. No concrete callback receiver or direct
scene-manager clear or request is recovered on this path, so it remains
insufficient evidence for a mission launch.
