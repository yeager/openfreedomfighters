# MovieControl-to-cutscene dispatcher recovery boundary

Status: behaviorally established at the private-observation boundary; native
runtime integration remains unsupported. A reviewed clean-room contract now
establishes the admitted controller route to the first-cut player, but it does
not authorize connecting authored runtime models until the retained
source-free trace has been independently reviewed.

## What is established

- MovieControl's ordinary event-16 model requires phase-one completion and a
  passed deadline before preparation. The normal lifecycle route also records
  whether phase two completed before that update.
- A successful preparation delivers directly and synchronously from the
  constructed MovieControl owner to the constructed first-cut sequence owner.
- The reviewed first-cut player can receive that delivery, enter its initial
  active state, and continue through its live receiver services.
- `CutSequenceCoordinator` and `MovieControlFirstUpdate` are authored,
  disconnected models. Their direct service calls remain non-evidence and do
  not enable normal intro playback.

## Required private observation

A private observer must establish both successful and failing paths from one
admitted MovieControl event to the selected first-cut player. It must show:

1. the observer-local callback order and event-16 gate result;
2. constructed MovieControl and sequence owner/component relations;
3. that the delivery sender is the MovieControl owner and the target is the
   constructed sequence owner, without exporting either identity;
4. whether a delivered handoff is synchronous, delivery outcome, first
   player-activation outcome, all component/owner
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
callback ordinal, four construction relations, phase-one and phase-two
relations, 32-bit status masks, event gate, sender/target relations,
categorical handoff, delivery-mode and activation states, outcome, and
external-service entry. A synchronous delivery is accepted only for a
successful delivered handoff from an admitted, source-bound MovieControl
relation to the constructed sequence-owner relation. A player start is accepted
only after that delivered source-bound handoff.

Keep raw and sanitized observations private. Only a reviewed source-free
behavior specification and authored tests may subsequently connect the runtime
path.

## Repeat-pair gate

`tools/movie_control_cutscene_dispatch_repeat_pair.py` validates two already
sanitized observations from fresh processes. It never accepts a raw observer
record and revalidates both inputs through the narrow dispatcher schema. The
two full structural traces must match exactly, including order, lifecycle
phase, callback ordinal, construction relations, status masks, event gate,
handoff/delivery/activation state, outcome, and external-service state. The
trace must also reach one terminal boundary: either a started player or an
explicit failed event, handoff, or activation boundary.

Run this gate separately for the successful route and for each observed failure
route. It establishes repeatability only; it does not turn an observation into
a native integration contract or bypass normal startup's fail-closed gate.

```sh
python3 tools/movie_control_cutscene_dispatch_repeat_pair.py \
  FIRST_SANITIZED.json SECOND_SANITIZED.json PAIR_RESULT.json
```

All three paths must be outside this repository, distinct, and the output must
not already exist. Keep the pair result private with the two sanitized inputs.
