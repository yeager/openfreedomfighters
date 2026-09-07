# Simulation replay boundary

Replay is project-authored deterministic infrastructure. It is not compatible
with retail recordings, save files, scripts, resources, paths, or presentation
state.

## Command capture

An attached, non-copyable world command capture starts by retaining the exact
portable `SimulationWorld` snapshot. It records only successfully accepted
between-tick queue mutations. Each record contains the completed world tick,
a capture-local ordinal spanning every command type, and the complete accepted
payload: spawn state plus request ID, destroy identity, or event fields plus
sequence ID.

The capture preflights fixed capacity before a world mutation, then receives a
non-throwing accepted notification immediately afterward. This prevents a
recording failure from leaving a mutated world with a missing record. Rejected
queue operations have no record. Reset and successful snapshot import terminate
an active capture rather than becoming implicit replay mutations.

Destroy operations do not use the world sequence allocator, so replay order
must use the separate capture-local ordinal. Playback applies every command
whose completed tick is `N` in ordinal order before stepping the input for tick
`N + 1`, and checks the request or event ID returned by the world.

## In-memory playback

The project has an in-memory recorder and atomic playback path. Playback first
imports the initial snapshot into an isolated world, validates the complete
input/checkpoint and command grammar, then applies command groups and input
steps there. It replaces the destination only when every checkpoint matches.
Malformed streams therefore leave the destination unchanged.

The grammar requires consecutive input/checkpoint ticks, command ordinals
starting at one without gaps, nondecreasing command boundaries, and command
boundaries from the initial tick through the tick before the final input.
Each command must contain only the payload for its declared kind. This makes
the in-memory representation canonical before a binary envelope exists.

Binary replay envelopes and command-stream serialization remain separate work.
