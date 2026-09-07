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

Binary replay envelopes, command-stream serialization and playback are separate
work. They must validate all data into a temporary world and replace an active
world only when replayed checkpoints match.
