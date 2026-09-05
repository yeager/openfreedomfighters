# Campaign compatibility contract

This document defines the evidence required for Phase 4. It is a compatibility
contract, not a claim that the campaign is implemented. Asset discovery or parser
coverage alone cannot establish mission behavior.

## Scope

The compatibility surface includes every campaign mission, rebel-base segment,
cutscene, difficulty, secret, unlock, persistence transition, profile operation,
save operation, and original input mapping exercised by the supported Steam data.
Original and Modern use the same authoritative simulation. Presentation settings
must not change mission state or checkpoint hashes.

## Behavior specification inventory

Each campaign segment must have a behavior-only specification derived under the
clean-room protocol. Its inventory entry records a stable project-authored segment
ID, prerequisites, initial state, objectives, scripted transitions, failure and
success conditions, persistence effects, and the evidence state for each target
platform. Retail names or extracted text are recorded only when necessary for
interoperability and safe to publish.

The public coverage states are `unscoped`, `specified`, `implemented`,
`replay-verified`, and `campaign-verified`. A segment cannot advance from asset
coverage alone. Unknown behavior stays explicit rather than receiving a guessed
implementation.

## Replay and checkpoint evidence

Every campaign-critical transition requires a versioned replay fixture containing
an independently authored initial-state description and tick-addressed actions.
Checkpoints use the canonical authoritative state hash defined by the
[simulation runtime](SIMULATION_RUNTIME.md). Evidence records the engine revision,
data-build fingerprint, profile, platform, architecture, and pass/fail result;
retail payloads, screenshots, extracted dialogue, and private traces remain out of
the public repository.

A replay pass proves only the declared transition and initial state. Phase 4 also
requires fresh-start end-to-end campaign runs on Windows, Linux, Steam Deck
hardware, and macOS.

## Saves, profiles, and migration

The portable save schema must be versioned, bounded, atomically written,
recoverable after interrupted writes, and covered by round-trip, corruption, and
migration tests. It records enough authoritative state to resume without deriving
gameplay state from presentation settings. Profile data and campaign save data
have separate ownership and validation boundaries.

Retail-save import is optional. If implemented, it is a one-way, isolated parser
for user-owned input and never changes the portable on-disk contract. No retail
save sample may enter Git history or CI.

## Platform acceptance matrix

For each target, evidence must name the OS version, architecture, native runtime,
build revision, input device, graphics backend, and result. Windows, Linux, and
macOS require native executables. Steam Deck requires a native Linux hardware run;
Wine, Proton, Rosetta, or another compatibility layer does not satisfy the gate.

Completion requires all of the following:

1. Every inventory entry is `campaign-verified` with no compatibility-critical
   divergence.
2. Every critical transition has a replayable regression and expected checkpoint.
3. A fresh-start campaign completes on all four targets in Original mode.
4. Original and Modern produce identical authoritative hashes for shared inputs.
5. Save/profile round trips, migrations, corruption handling, and recovery pass.

## Current status and unknowns

Phase 4 has not started as a campaign implementation and its gate is open. The
portable scheduler, entity lifetime, event queue, and canonical checkpoints are
foundational infrastructure only. Mission behavior specifications, campaign
inventory, script runtime, complete gameplay systems, save/profile schema,
original input semantics, replay corpus, and end-to-end platform evidence remain
unimplemented or unproven. The authoritative checklist is the
[roadmap](ROADMAP.md).
