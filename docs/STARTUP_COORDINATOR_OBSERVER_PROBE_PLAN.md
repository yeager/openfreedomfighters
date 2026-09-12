# Startup coordinator opaque observer plan

This private workflow is observation preparation only. It cannot locate an
installation, select a process, attach a debugger, or supply executable data.

`tools/startup_coordinator_observer_probe_plan.py` canonicalizes exactly five
fixed protocol positions: coordinator entry, root selection, camera/view,
pass delivery, and coordinator exit. They are protocol categories, not target
names, symbols, offsets, paths, identities, or debugger instructions. The
validator accepts no free-form values.

`tools/startup_coordinator_observation_runner.py` starts an explicitly supplied
private observer only with `--execute`, a canonical plan, a new private
workspace, and literal `fresh-isolated` mode. It has no target discovery or
attach interface; observer stdout/stderr are discarded. The observer must emit
exactly two bounded structural records: one completed selected pass and one
rejected pass. The runner validates both through the existing strict trace
schema, requires distinct observer-local callback ordinals, removes raw records
on success, timeout, failure, and schema rejection, and keeps only sanitized
records. It does not print their contents or paths.

```sh
python3 tools/startup_coordinator_observation_runner.py --execute \
  --observer PRIVATE_OBSERVER_EXECUTABLE \
  --canonical-plan PRIVATE_CANONICAL_PLAN.json \
  --workspace NEW_PRIVATE_WORKSPACE \
  --timeout-seconds 300
```

This wrapper is a boundary guardrail, not proof that an external observer was
fresh or isolated. A separately reviewed native behavior contract is still
required before any normal coordinator, scene, camera, renderer, or intro path
is enabled.
