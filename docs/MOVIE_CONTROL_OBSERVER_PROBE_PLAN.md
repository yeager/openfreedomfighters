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

## Fresh-process collection runner

`tools/movie_control_observation_runner.py` is an optional private wrapper for
one explicitly requested observation. It does not include an observer,
instrumentation, game path, process ID, attach selector, debugger expression,
or target-discovery behavior. The separately maintained private observer is
given only a canonical opaque plan and a new private workspace. It must create
exactly the two expected structural records: phase-one and MovieControl-to-
first-cut dispatch.

The wrapper starts nothing unless `--execute` is present. It invokes the
private observer with the literal `fresh-isolated` mode, uses no shell, hides
observer stdout and stderr, and never forwards raw records to a terminal. It
then validates both records through the strict source-free schemas, deletes the
raw forms, and retains only their sanitized structural forms in the private
workspace. An extra file, symlink, pre-existing workspace, malformed plan, or
unsupported trace field fails collection.

```sh
python3 tools/movie_control_observation_runner.py --execute \
  --observer PRIVATE_OBSERVER_EXECUTABLE \
  --canonical-plan PRIVATE_CANONICAL_PLAN.json \
  --workspace NEW_PRIVATE_WORKSPACE
```

The wrapper cannot prove what an external private observer did internally; it
is a guardrail, not evidence that a real process was fresh or isolated. The
operator must keep the observer and workspace private and must never attach to,
stop, or otherwise alter a running original process. A completed collection is
only candidate evidence for the separate phase-one and dispatch repeat/failure
reviews; it does not enable native intro playback.
