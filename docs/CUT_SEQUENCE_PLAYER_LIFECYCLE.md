# CutSequence player lifecycle recovery boundary

Status: unsupported. The retained `ZLIST_CutSequence` source can construct the
reviewed first-cut player and its private list receiver, but that does not
prove player admission, clock activation, or completion.

## What is established

- The sequence component, first-cut list, five commands, camera relation and
  legal-picture relation have individually bounded reader provenance.
- `FirstCutPlayerInitialization` has a source-specific cold two-phase model:
  phase one visits commands in reverse order and phase two performs the
  checked member/reference sweep before sealing its private receiver.
- The receiver model is not the external-fade list and cannot accept a command
  before phase one or after phase two.

Those facts do not establish that `ZLIST_CutSequence` dispatches either phase,
activates the player after phase two, consumes a scene clock, drives a view,
or reports completion. Normal startup must remain fail-closed.

The upstream MovieControl-to-player handoff has its own source-free observation
boundary; see [MovieControl-to-cutscene dispatcher recovery](MOVIE_CONTROL_CUTSCENE_DISPATCH.md).

## Missing contract

Private observation must establish the real player dispatcher and both success
and failure paths, including:

1. reader receipt and owner/component state before phase one;
2. ordering and state effects of both initialization phases;
3. member and reference sweep outcomes, without exporting their identities or
   values;
4. the sealed-receiver boundary relative to first activation;
5. whether activation actually starts a player, plus completion ownership and
   status effects; and
6. every external-service entry and failure boundary.

## Private structural trace utility

`tools/cut_sequence_player_lifecycle_trace.py` validates a deliberately
source-free trace from a separately maintained private observer. It accepts
only local order/callback ordinals, construction and reader relations, 32-bit
status masks, categorical player/receiver/member/reference/activation/
completion states, outcome and external-service entry. It rejects object IDs,
member IDs, references, timing values, paths, strings, assets, bytes,
screenshots, executable material, addresses, offsets and symbols.

It does not open or instrument original binaries, archives or game files. Both
input and output stay outside this repository, and it refuses to overwrite an
existing output.

```sh
python3 tools/cut_sequence_player_lifecycle_trace.py PRIVATE_INPUT.json PRIVATE_OUTPUT.json
```

Only a reviewed source-free behavior specification that closes this contract
may enable the first-cut player in normal startup.
