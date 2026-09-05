# Gameplay simulation execution specification

This is the Phase 3 implementation and acceptance contract. It is not a claim
that retail gameplay has been recovered. Today the project has only the
fixed-step scheduler, tick-addressed input, deterministic entity lifetime,
future-tick events, and canonical checkpoints described in
[SIMULATION_RUNTIME.md](SIMULATION_RUNTIME.md).

Public implementation follows the [clean-room protocol](../CLEAN_ROOM.md).
Public tests use independently authored fixtures. Private comparison may use a
verified user-owned installation, but never publishes retail code, data,
dialogue, media, or expressive traces.

## Vertical-slice boundary

The selected first-mission segment must have a stable project-authored ID and a
behavior-only contract recording its initial state, supported difficulty,
tick-addressed actions, mission transitions, lifecycle events, interactions,
damage, animation transitions, and persistence effects. Unknowns remain explicit;
missing evidence is never replaced by guessed compatibility behavior.

Completion requires locomotion, one weapon, damage and death, one interaction,
navigable AI, squad recruitment and an order, mission success or failure, and a
save/resume boundary. A narrower demonstration does not pass Phase 3.

## Authoritative state

Gameplay state is pointer-free, fixed-width, bounded, and owned by the simulation
world. Components use generational `EntityId`; stale IDs never bind reused slots.
Presence, iteration, lifecycle/event ordering, and system order are versioned
determinism contracts.

| Group | Authoritative fields |
| --- | --- |
| Transform | quantized position/orientation, previous transform, parent |
| Character | stance, locomotion, health, faction, inventory, active item |
| Weapon | ammunition, reload, cooldown, spread/RNG stream, owner |
| Interaction | target, activation, cooldown, mission event |
| AI | state, target, perception memory, path request, decision timers |
| Squad | leader, ordered members, order, morale and charisma inputs |
| Animation | logical state, fixed-point phase, transition, root motion |
| Collision | shape, layer/mask, contacts and grounded state |
| Mission | script instance, objectives, counters, timers, persistence keys |

Meshes, particles, audio voices, GPU resources, wall time, localized text, window
state, and graphics profile are non-authoritative. They consume immutable tick
results and cannot feed gameplay decisions back into the world.

Storage imposes limits before allocation or mutation. Systems use stable entity
or command order, never hash-container order. Authoritative floating point is
allowed only where supported targets prove stable results; otherwise specified
integer/fixed-point operations define rounding, overflow, and saturation.

## Tick order

Recovered evidence may refine this provisional sequence. A change bumps the
checkpoint contract and replay migration:

1. Validate and consume exactly one next-tick input snapshot.
2. Apply lifecycle commands at the documented boundary.
3. Deliver due mission/script events in sequence order.
4. Update mission state and enqueue deferred commands.
5. Resolve player intent, squad orders, AI perception, and decisions.
6. Advance navigation and locomotion intent.
7. Advance weapons, interactions, damage, and status.
8. Advance logical animation and bounded root-motion requests.
9. Resolve collision/physics and commit transforms and contacts.
10. Commit deferred gameplay commands, emit presentation events, and checkpoint.

Every boundary has testable input/output. Results cannot depend on render frames,
async completion, host locale, filesystem order, thread scheduling, or wall time.
Parallel read-only work commits results in deterministic order.

## Mission and script runtime

Scripts use a small versioned host API, never direct engine pointers. Instances
have bounded resource references and local state, deterministic timers/RNG, and
an instruction budget per tick. Host calls enqueue validated entity, objective,
presentation, and persistence commands. Invalid opcodes, handles, recursion,
counts, or budget exhaustion produce structured failure; scripts have no arbitrary
file access.

Implementation proceeds by recovering the container/bytecode boundary,
publishing behavior-only opcode and host-call contracts, adding a bounded decoder
and validator, then implementing only the selected segment's proven surface.
Unknown operations remain unsupported and diagnostic, never silently ignored.

## Gameplay systems

Locomotion separates intent, constraints, collision, and final transform. Its
contract records acceleration, speed, stance, slope/step/airborne behavior, turn
rules, and camera-relative input mapping. Camera rendering is non-authoritative;
the quantized aim direction supplied to gameplay is authoritative.

Weapon contracts define eligibility, ammunition, reload, cadence, projectile or
hit-scan behavior, spread/RNG consumption, collision masks, and damage ordering.
Damage specifies simultaneous-hit order, health, death, invulnerability, faction
filtering, and mission notification. Interaction specifies selection,
range/angle, contention, activation, cancellation, and emitted events.

Navigation consumes validated immutable spatial data, has bounded work queues and
stable ties, and tags results with request tick and entity generation. Late/stale
results are discarded. AI separates perception, memory, decision, navigation,
and action; contracts record cadence, ranking, transitions, reaction, cover, and
fallback. Modern graphics cannot affect perception.

Squad membership is ordered and bounded. Recruitment, dismissal, leadership,
orders, acknowledgement, targets, morale, and charisma define simultaneous-event
order. Missing presentation cues never change the underlying state.

Logical animation belongs to simulation; render-only sampled bones do not. The
contract defines clip identity, transition priority, fixed-point advancement,
loops, notifications, and root-motion quantization. Collision contracts cover
shapes, masks, broad/narrow phases, contact sorting, depenetration, grounding,
slopes, steps, triggers, and rays. Physics is project-controlled unless a
dependency is proven deterministic across every target. Ties use stable IDs, not
addresses or thread completion.

## Saves, profiles, replay, and migration

The portable save is independent of the retail format. Its envelope contains
magic, schema and engine-compatibility versions, byte order, bounded section
directory, lengths/checksums, supported-data fingerprint, project-authored
campaign ID, authoritative tick/RNG/entity/component/mission/script state,
commit generation, and whole-document integrity.

It never stores pointers, native structure dumps, render state, absolute paths,
retail payloads, localized prose, or machine identifiers. Readers validate all
arithmetic, counts, lengths, references, versions, and checksums before building
live state. Unknown optional sections are skipped by length; unknown required or
incompatible versions fail without mutating the world.

Writes use a same-directory staging file, flush contents, atomically replace when
the platform supports it, and retain a previous valid generation until durable.
Recovery selects only a fully validated generation. Profiles and campaign saves
have separate schemas; graphics settings never enter campaign state.

Each supported version needs immutable project-authored golden fixtures,
round-trip and every-byte truncation tests, hostile lengths/counts, checksum and
unknown-section tests, interruption recovery, and migration tests. Migrations are
sequential, idempotent, bounded, preserve the source until success, and never
downgrade. Retail-save import is optional, isolated, and one-way.

Replays contain a declared initial-state reference, simulation-contract version,
ordered tick input/external deterministic events, and expected checkpoints--never
retail resources. Incompatible contracts or fingerprints are rejected rather
than approximated.

## Delivery and evidence

1. Freeze the segment inventory and authoritative-field registry.
2. Add bounded components, deterministic RNG streams, and system checkpoints.
3. Specify and implement the selected mission/script surface.
4. Implement gameplay systems in dependency order.
5. Implement save/profile persistence, recovery, migration, and replay.
6. Capture private retail-backed traces and publish only non-retail fixtures and
   aggregate comparison reports.
7. Run identical replay/checkpoint suites on every target and presentation mode.

Public CI covers bounds, malformed input, ordering, stale IDs, RNG consumption,
save recovery/migration, and replay divergence under sanitizers. Private evidence
records data fingerprint, revision, method, schema, platform/architecture, and
aggregate result without retail content.

The gate passes only when the complete segment and save/resume replay on Windows,
Linux, Steam Deck hardware, and macOS; checkpoints meet versioned expectations;
and Original and Modern have identical authoritative hashes. It is not met today.
