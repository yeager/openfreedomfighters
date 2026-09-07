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
