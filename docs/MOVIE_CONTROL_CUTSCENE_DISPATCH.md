# MovieControl-to-cutscene dispatcher recovery boundary

Status: unsupported. The checked MovieControl reader and the cold first-cut
player reader identify the two ends of a possible route, but do not prove that
an admitted MovieControl event dispatches to that player, which receiver is
used, or that the player starts.

## What is established

- MovieControl's ordinary event-16 model requires phase-one completion and a
  passed deadline before its disconnected preparation boundary.
- The reviewed first-cut player can be constructed and cold-initialized with a
  sealed private receiver.
- `CutSequenceCoordinator` and `MovieControlFirstUpdate` are authored,
  disconnected models. Their direct service calls are not evidence of the
  original dispatcher and do not enable normal intro playback.

## Required private observation

A private observer must establish both successful and failing paths from one
admitted MovieControl event to the selected first-cut player. It must show:

1. the observer-local callback order and event-16 gate result;
2. constructed MovieControl and sequence owner/component relations;
3. that the delivery sender is the MovieControl owner and the target is the
   constructed sequence owner, without exporting either identity;
4. delivery outcome, first player-activation outcome, all component/owner
   status transitions, and external-service entry; and
5. the phase-one-completion relation used by the admitted event.

This evidence deliberately does not establish clock units, command content,
camera selection, presentation, completion behavior, or a generic dispatch
mechanism. Normal startup remains fail-closed.

## Private structural trace utility

`tools/movie_control_cutscene_dispatch_trace.py` accepts only the structural
relations above from a separately maintained private observer. It rejects all
unrecognized fields, including object IDs, references, timing values, strings,
paths, assets, bytes, screenshots, addresses, offsets and symbols. It does not
open or instrument original binaries, archives, game files, logs, or dumps.
Input and output must be outside this repository, and output is never
overwritten.

```sh
python3 tools/movie_control_cutscene_dispatch_trace.py PRIVATE_INPUT.json PRIVATE_OUTPUT.json
```

The accepted raw format is `off.movie-control-cutscene-dispatch.raw/v1`. Each
event has a strictly increasing observer-local order, non-decreasing phase
(`event16`, `handoff`, `player_activation`, `completion`, or `failure`), local
callback ordinal, four construction relations, phase-one relation, 32-bit
status masks, event gate, sender/target relations, categorical handoff and
activation states, outcome, and external-service entry. A player start is
accepted only after a successful delivered handoff from an admitted,
phase-one-complete constructed MovieControl relation to the constructed
sequence-owner relation.

Keep raw and sanitized observations private. Only a reviewed source-free
behavior specification and authored tests may subsequently connect the runtime
path.
