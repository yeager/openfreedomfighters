# MovieControl opaque observer probe plan

Status: observation preparation only. This is not a MovieControl
implementation contract and does not enable normal startup.

`tools/movie_control_observer_probe_plan.py` validates the smallest private
plan that a separately maintained observer may accept before collecting a
source-free phase-one record. It does not discover an installation or target,
open a game file or executable, attach a debugger, or implement instrumentation.

## Fixed protocol

The raw JSON input has exactly two fields:

- `format`, exactly `off.movie-control-observer-probe-plan.raw/v1`;
- `probes`, exactly four objects with these slot/point relations:
  `{"slot": 0, "point": "global_phase_one_enter"}`,
  `{"slot": 1, "point": "candidate_enter"}`,
  `{"slot": 2, "point": "candidate_leave"}`, and
  `{"slot": 3, "point": "global_phase_one_leave"}`.

The point labels are protocol positions only. They are not executable names,
addresses, offsets, symbols, paths, locator rules, game strings, asset
identities, debugger expressions, or raw observed material. There are no
optional fields or free-form values. A plan with a missing, repeated,
malformed, or additional probe is rejected. Input order is normalized by slot
in the canonical output.

The canonical output changes only the format marker to
`off.movie-control-observer-probe-plan/v1`; it retains the fixed probe list. The
validator never creates target-specific information.

## Private use

Both input and output must be outside the repository, must differ, and the
output must be new. The command refuses an existing output rather than
overwriting it:

```sh
python3 tools/movie_control_observer_probe_plan.py \
  PRIVATE_PLAN.json PRIVATE_CANONICAL_PLAN.json
```

Keep both files private. This gate is only preparation for the existing
phase-one source-free trace process in
[MovieControl phase-one recovery](MOVIE_CONTROL_PHASE_ONE.md). A validated
plan neither identifies an observer target nor proves a callback relation,
failure behavior, or any gameplay behavior.
