# Private startup coordinator pass contract

The private observation workflow may emit one source-free structural receipt at
`reviewed-startup-coordinator-pass/reviewed-startup-coordinator-pass.json`
beneath application preferences. The v1 receipt records two matching completed
coordinator passes and one matching rejected pass, with only fixed categories
and booleans. It contains no retail text, asset names, paths, timings,
addresses, handles, callback identities, bytes, or screenshots.

`ReviewedStartupCoordinatorPassContract::load_local` accepts only a direct,
regular, non-symlink child with the exact bounded v1 JSON schema. It rejects
unknown or duplicate fields, malformed categories, unsupported JSON features,
trailing bytes, oversized files, and symlinks.

The resulting object is deliberately inert. It retains only admission, creates
no runtime identifiers, and is not passed to a coordinator, scene, camera,
renderer, or normal startup. A separately reviewed native behavior contract is
required before any coordinator pass can run.

Use `tools/startup_coordinator_repeat_observation_runner.py` to collect the
two fresh-isolated observer runs and produce the receipt in one bounded private
operation. It independently repeat-gates both the completed and rejected
records, then revalidates the completed pair and one rejected record through
the receipt bundler. It produces the exact fixed receipt schema above; it does
not preserve trace payloads in the receipt.

```sh
python3 tools/startup_coordinator_repeat_observation_runner.py --execute \
  --observer PRIVATE_OBSERVER_EXECUTABLE \
  --canonical-plan PRIVATE_CANONICAL_PLAN.json \
  --first-workspace NEW_PRIVATE_WORKSPACE_A \
  --second-workspace NEW_PRIVATE_WORKSPACE_B \
  --output-directory PRIVATE_RECEIPT_DIRECTORY
```
