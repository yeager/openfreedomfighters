# Startup BootMenu boundary

`StartupBootMenuAdmission` models a narrow component-initialization boundary
for a factory-produced FF-StartUp BootMenu component. It requires a live owner,
a live identity registry, two opaque resolved identities, and one successful
retained routing operation. When all checks succeed, `initialized()` is true.

This is not input admission. The first resolved identity is opaque; it is not a
key, controller button, action-map entry, or proof that the native component
handler accepts it. `observe()` only records an already supplied matching
opaque identity. It does not dispatch a handler, create focus, select a menu
item, request a scene, or make a retail menu interactive.

The source-backed construction token remains a separate earlier boundary. It
proves the ordinary-window owner and exact `ZWINDOW_BootMenu` attachment exist
in one scene transaction; it does not complete owner/component readers or this
initialization step.
