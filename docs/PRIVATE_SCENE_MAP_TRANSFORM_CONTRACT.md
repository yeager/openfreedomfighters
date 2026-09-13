# Private Scene Map-Transform Contract

`scene_map_transform_trace.py` accepts output from an externally maintained,
fresh isolated observer. It does not inspect game data, executables, memory,
screenshots, logs, or process state.

The raw input may contain only ordered categorical relation states and one
observer-local callback ordinal. It cannot contain identifiers, names, asset
references, paths, addresses, bytes, matrices, vectors, or scalar transform
values. The sanitized trace is owner-only, bounded, no-follow, and must live
outside the repository.

`scene_map_transform_contract_bundle.py` requires two byte-identical successful
traces covering entry, parent relation, local relation, world resolution, render
consumption, and completion exactly once, plus a separate failure trace. It
emits only an inert categorical receipt. No runtime code reads this receipt yet.

This evidence is necessary but not sufficient to render C03A as gameplay. A
native map-transform implementation remains disconnected until the scene
activation and camera lifecycles are independently reviewed.
