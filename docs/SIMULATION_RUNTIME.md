# Portable simulation runtime

The current simulation runtime is project-authored deterministic infrastructure.
It is not a claim about the original game's entity system, event model, update
order, or serialization format.

## Timing and input

`FixedStepScheduler` represents elapsed time in integer phase units at a rational
tick rate. It bounds long stalls and reports discarded backlog explicitly.
Interpolation is presentation-only. `InputAccumulator` converts platform events
into tick-addressed snapshots with persistent held state and analog axes plus
single-consumption press and release edges. Focus loss releases held actions and
neutralizes axes.

Original and Modern presentation modes must advance the same simulation world
with the same ordered snapshots. Refresh rate, rendering mode, frame limits, and
graphics menus are outside authoritative state.

## World ownership

`SimulationWorld` owns pointer-free authoritative state. Entities use an index
and generation pair, so destroying and reusing a slot invalidates stale IDs.
Slots whose generation can no longer advance are retired instead of wrapping.
Live entities are exposed in ascending slot order.

Spawns and destroys are queued between ticks. At the next `step`, valid destroys
are applied in request order, duplicate or stale destroys have no effect, then
spawns are applied in request order using the lowest reusable slot. This ordering
is an explicit portable policy for the vertical slice, not recovered retail
behavior. Every step requires the next consecutive input tick. Entity and queue
counts have explicit hard limits, and capacity is reserved before mutation.

Events target a future simulation tick and receive a monotonic sequence number
from the world. Events for the current tick are delivered in sequence order.
Event type and payload values are fixed-width integers; their gameplay meanings
remain future system contracts. Entity IDs in an event remain generational and
are never silently rebound to a reused slot.

## Deterministic checkpoints

`state_hash` computes SHA-256 over a version-tagged, explicitly little-endian
canonical stream. It includes the current tick, sequence allocator, last input,
configured world limits, complete slot extent and generations, authoritative
entity values, queued lifecycle operations, and future events. It never hashes
structure padding, container capacity, pointers, wall-clock time, renderer
state, or menu state.

The digest is intended for deterministic checkpoint comparison. Its byte-stream
version is an internal compatibility boundary and must change if canonical field
meaning or ordering changes.

## World snapshots

`SimulationWorld` can export and import a project-authored portable snapshot of
the complete state covered by its checkpoint: limits, clock and sequence state,
last input, slots (including dead-slot data), and all pending queues. The format
uses explicit little-endian fields, a fixed versioned envelope and a SHA-256
payload checksum. Import verifies all declared limits, sequences, generations,
future-event ticks, checksum and complete input consumption into a temporary
world before replacing live state.

This is deterministic infrastructure for future save and replay envelopes. It
is not a retail-save format, does not contain presentation or game assets, and
does not make the project compatible with the original game's serialization.

## Planned durable save store

The future project save store will wrap this snapshot in a separate versioned
envelope and retain two same-directory generations. Loading will validate both
copies into staged worlds, choose only an unambiguously newest valid generation,
and leave files untouched. Saving will write and flush an exclusive sibling
temporary before replacing only the older copy. Corrupt, truncated, mismatched,
or ambiguous equal-generation files must fail without mutating the destination.
This is portable project persistence, not retail-save import.

The required ordered command-capture boundary for future replay is specified in
[Simulation replay](SIMULATION_REPLAY.md).
