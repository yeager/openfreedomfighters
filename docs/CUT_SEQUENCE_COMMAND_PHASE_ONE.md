# CutSequenceCommand phase-one recovery boundary

Status: unsupported. `ZLIST_CutSequenceCommand` has a reader-shaped boundary,
but no admitted lifecycle callback. A generic list reader or a synthetic
completion transition would make the intro look ready without proving the
original command path.

## Required evidence

Before phase one can enter native startup, one private observation must establish:

1. The construction and complete reader-graph receipt for the selected command.
2. The non-decreasing lifecycle phase order and observer-local callback order.
3. Whether registration was attempted, whether it succeeded, and whether it
   occurred before or after target delivery.
4. Whether a callback delivered to a target or only touched contextual state.
5. Component and owner status transitions, external-service entry, and both
   success and failure results.

All identities are relations, not IDs. The observation must not export game
data, executable material, strings, paths, assets, bytes, screenshots,
addresses, offsets, symbols, or retail values.

## Private structural trace utility

`tools/cut_sequence_command_trace.py` validates the deliberately small record
format emitted by a separately maintained private observer. It does not inspect
or instrument the game. The raw input and sanitized output must both be outside
this repository, and an existing output is never overwritten.

Each event has increasing observer-local order, one non-decreasing phase
category (`construction`, `reader`, `phase_one`, `registration`, `delivery`,
`completion`, or `failure`), a local callback ordinal, construction/reader
relations, 32-bit status masks, registration and delivery categories, outcome,
and external-service entry. Delivery distinguishes `context_only` from
`target_delivered`; the validator rejects contradictory registration order.

On the private observation host:

```sh
python3 tools/cut_sequence_command_trace.py PRIVATE_INPUT.json PRIVATE_OUTPUT.json
```

Keep both files private. A public implementation may use only a reviewed,
source-free behavior specification and authored tests. Normal startup remains
fail-closed until the full contract is established.
