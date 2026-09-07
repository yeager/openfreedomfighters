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
