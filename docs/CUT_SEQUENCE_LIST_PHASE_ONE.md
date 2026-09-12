# CutSequenceList phase-one recovery boundary

Status: unsupported. `ZLIST_CutSequenceList` has a bounded retained-list model
and a reader boundary, but no recovered lifecycle dispatch. The list is
distinct from `CutSequenceCommand`: its contract must account for collection
state and the window in which external commands can join it.

## What is established

- A concrete source-65 list is constructed before its bounded reader boundary.
- It retains an ordered command container used by the already isolated external
  fade-command path.
- Application-level construction has a separate collection registration path.

These are construction and reader facts, not evidence that phase one opens
command admission, changes a status bit, calls an external service, or creates
or retains an application collection. In particular, the source-65 list is not
the separately recovered first-cut player.

## Missing contract

Before the list can participate in normal startup, private observation must
identify the phase-one dispatcher callback and establish, for successful and
failing paths:

1. its order relative to the complete reader receipt and later list callback;
2. required component/owner state and every component/owner status transition;
3. collection state before and after the callback;
4. whether and when the external-command admission window opens or closes; and
5. each external-service entry and callback failure boundary.

A generic `run_phase_one`, an unconditional collection allocation, or opening
the current command container based only on construction would create a native
path without proving the original behavior. Normal startup therefore remains
fail-closed.

## Private structural trace utility

`tools/cut_sequence_list_phase_one_trace.py` validates a deliberately small
trace from a separately maintained private observer. It never opens or
instruments original binaries, archives, assets, debugger logs, dumps, or
screenshots. Input and output must both be outside this repository and an
existing output is never overwritten.

The only accepted fields are observer-local order/callback ordinal, phase,
constructed component/owner relations, reader receipt, 32-bit before/after
status masks, categorical collection state, categorical command-window state,
success/failure, and external-service entry. The schema contains no object
identifiers, command content, strings, paths, addresses, offsets, symbols,
assets, bytes, screenshots, or retail values.

On the private observation host:

```sh
python3 tools/cut_sequence_list_phase_one_trace.py PRIVATE_INPUT.json PRIVATE_OUTPUT.json
```

Keep both trace files private. A public implementation may be added only after
reviewing a source-free behavior specification that accounts for every state
change and failure path.
