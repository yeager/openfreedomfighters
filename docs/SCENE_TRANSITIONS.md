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

The later manager pump, archive selection, lifecycle handoff, and `FF-StartUp`
menu construction are not implemented. The source of LoadScreen's initial
counter and setup flag also remains a separate construction requirement.

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
