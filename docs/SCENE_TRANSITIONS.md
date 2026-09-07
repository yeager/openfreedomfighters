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
