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
meaning or ordering changes. Save-game serialization remains a separate future
contract.
