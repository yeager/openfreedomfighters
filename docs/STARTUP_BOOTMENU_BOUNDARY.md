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

Private clean-room tracing has established a separate `STARTGAME` to
`PressStart` notification/subscription boundary. It has not established a
playable-scene target, a scene-manager request, or a main-menu selection in
that chain. Public code must therefore not bind a key, controller button, F10
action, or `PressStart` notification directly to a guessed level. The next
required trace follows the notification recipient and the reached owner route.

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
