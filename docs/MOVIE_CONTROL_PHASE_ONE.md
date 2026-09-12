# MovieControl phase-one recovery boundary

Status: unsupported. The retained owner and component readers prove one
MovieControl identity and make the phase-two callback eligible for a future
global pass. They do not prove that phase one is empty, nor do they identify a
phase-one callback.

## What is established

- The controller has one source-bound owner, one source-bound component, and
  the checked reader receipts described in
  [COMPONENT_LIFECYCLE.md](COMPONENT_LIFECYCLE.md).
- Its later phase-two callback has a separately reviewed service contract.
- The later ordinary event path uses the fixed phase-two deadline and requires
  phase-one completion before it can prepare the first cut.

None of those facts establish the preceding lifecycle dispatch. In particular,
the component's requested mask is an admission request, not evidence that a
given callback ran or that its owner transition is safe to synthesize.

## Missing contract

The following facts are required before phase one can be implemented or added
to normal startup:

1. The exact dispatcher entry that selects the MovieControl component for
   phase one, including its order relative to other components.
2. The concrete callback identity and its required owner, reader, event and
   external-service preconditions.
3. Every observable callback effect: component and owner status changes,
   event enrollment or removal, retained-state changes, and failure behavior.
4. The completion boundary used by the later event-16 gate. A synthetic
   completion bit would make the cut path appear ready without proving that
   the original callback's work occurred.

Private static review found an update-shaped routine consistent with the
already documented fixed-delay path, but the retained artifact does not link
that routine, or any candidate phase-one routine, uniquely to the
MovieControl factory. The surrounding base implementation is shared by other
visual classes. It is therefore insufficient evidence for a public callback
contract.

## Required private observation

Use an isolated, owned installation to record a source-free lifecycle trace
that correlates the constructed controller identity with its phase-one
dispatch. The trace must report only structural metadata: dispatch order,
phase number, component/owner identity relation, pre/post status masks,
event-membership change, ordinary-membership before/after, global-lifecycle
entry/completion/outcome, phase-one completion, success/failure, and whether an
external service was entered. It must not export identifiers, addresses,
offsets, paths, game strings, assets, payload bytes, screenshots or executable
material.

The implementation may proceed only when that trace identifies one callback
and all of its required effects. The resulting public tests must use authored
fixtures or source-free recordings; normal startup remains fail-closed until
then.

The subsequent MovieControl-to-first-cut handoff is a separate missing contract;
see [MovieControl-to-cutscene dispatcher recovery](MOVIE_CONTROL_CUTSCENE_DISPATCH.md).

## Private structural trace utility

Before a separately maintained observer is configured, its source-free,
opaque protocol plan may be checked with
[MovieControl opaque observer probe plan](MOVIE_CONTROL_OBSERVER_PROBE_PLAN.md).
That preparatory gate neither discovers a target nor supplies instrumentation.

`tools/movie_control_phase_one_trace.py` is a deliberately narrow sanitizer for
the output of a separately maintained private observer. It does **not** inspect
or instrument the original executable itself. That observer must assign
run-local callback ordinals and record only the fields above. Do not pass it
debugger logs, memory dumps, disassembly, screenshots, or exported game data.

The input and output paths are required to be outside this repository, and the
output is never overwritten. The accepted raw JSON format has exactly one
`format` value, `off.movie-control-phase-one.raw/v2`, and an `events` array.
Each event has only the following structural fields:

- increasing `dispatch_order`, `phase` (always `1`) and observer-local
  `callback_ordinal`;
- boolean relations to the constructed controller and owner, rather than raw
  identities;
- 32-bit pre/post component and owner masks;
- before/after event membership; and
- global-lifecycle entry and completion booleans with the categorical
  `not_observed`/`success`/`failure` outcome;
- before/after ordinary membership and phase-one completion; and
- `success`/`failure` plus `entered`/`not_entered` external-service state.

On the private observation host, create the raw record with the private
instrument and then run:

```sh
python3 tools/movie_control_phase_one_trace.py \
  PRIVATE_INPUT.json PRIVATE_OUTPUT.json
```

Review two fresh-process runs. A candidate is sufficient only if it ties the
constructed component to one callback ordinal, gives its relative dispatch
order, and accounts for every changed mask, event/ordinary-membership
transition, global-lifecycle entry/completion outcome, phase-one completion,
external-service entry, and failure path. Keep both raw and sanitized records
private; only a behavior specification and authored test fixture may be added
to this repository after review.

## Repeat-pair gate

`tools/movie_control_phase_one_repeat_pair.py` is the second private gate. It
accepts two already-sanitized outputs from the phase-one utility and writes one
new source-free result outside the repository. It never accepts raw observer
records and revalidates both input schemas before comparing them.

Each run must contain exactly one candidate with a constructed component and
its constructed owner, a completed successful global lifecycle, successful
phase-one completion, and a successful callback outcome. The two candidates
must agree exactly on the observer-local callback ordinal and dispatch order,
as well as every retained structural effect: component and owner status masks,
event and ordinary membership transitions, and external-service state. A
missing, incomplete, ambiguous, or mismatched candidate is rejected. This is
evidence for further review only; it does not authorize a native callback or
remove the normal-startup fail-closed gate.

On the private observation host, after producing two distinct sanitized files,
run:

```sh
python3 tools/movie_control_phase_one_repeat_pair.py \
  FIRST_SANITIZED.json SECOND_SANITIZED.json PAIR_RESULT.json
```

All three paths must be outside this repository; `PAIR_RESULT.json` must not
already exist. Keep the pair result private alongside its input observations.

## Failure-path contract bundle

The successful repeat pair does not establish failure behavior. Before a
reviewer may write a behavior specification, capture one separate failing run
with the same private observer and sanitize it with
`movie_control_phase_one_trace.py`. The failure record must identify the same
observer-local callback ordinal and dispatch order as the pair candidate, begin
with the same component/owner status and event/ordinary-membership state, enter
the global lifecycle, end that lifecycle with `failure`, and leave phase-one
completion false. It must not also report a successful invocation of that
constructed callback.

`tools/movie_control_phase_one_contract_bundle.py` revalidates the repeat-pair
result and sanitized failure trace, then writes one source-free structural
bundle. It accepts no raw observations and no identifiers, addresses, offsets,
symbols, strings, paths, game data, executable material, screenshots, or
payload bytes. All paths must be distinct and outside this repository; the
output must be new:

```sh
python3 tools/movie_control_phase_one_contract_bundle.py \
  PAIR_RESULT.json FAILURE_SANITIZED.json CONTRACT_BUNDLE.json
```

The bundle retains the successful candidate and the observed failure relation
only for private human review. It does not identify an implementation callback,
does not establish service semantics beyond the recorded structural state, and
does not authorize native phase-one code or normal startup.
