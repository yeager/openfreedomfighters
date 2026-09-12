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

Use `tools/startup_coordinator_pass_contract_bundle.py` only after collecting
two independently fresh isolated completed traces and one separate rejected
trace. It revalidates all three sanitized records, requires the completed pair
to agree exactly and requires the rejected trace to use a distinct
observer-local callback ordinal. It produces the exact fixed receipt schema
above; it does not preserve trace payloads in the receipt.

```sh
python3 tools/startup_coordinator_pass_contract_bundle.py \
  PRIVATE_SUCCESS_A.json PRIVATE_SUCCESS_B.json PRIVATE_REJECTED.json \
  PRIVATE_RECEIPT_DIRECTORY
```
