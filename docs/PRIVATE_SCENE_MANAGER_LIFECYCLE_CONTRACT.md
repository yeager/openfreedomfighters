# Private scene-manager lifecycle contract

`scene_manager_lifecycle_trace.py` accepts one private structural observation of
the scene-manager boundary. Its exact schema retains only phase ordering,
observer-local callback ordinal, categorical staging/lifecycle/camera/commit
results, and outcome. It rejects paths, addresses, identities, text, bytes,
timings, assets, and extra fields.

Both commands accept only bounded regular files outside the repository. They
refuse final symlinks, repository paths, non-existent or symlinked immediate
parents, duplicate paths, and pre-existing output. Output is created with
exclusive owner-only permissions, so the tools neither overwrite nor follow an
existing private result.

`scene_manager_lifecycle_contract_bundle.py` requires two identical successful
observations and a structurally separate failed observation. It emits only the
fixed inert receipt `off.scene-manager-lifecycle-contract/v1`; the receipt
does not preserve raw observations or any retail material.

Neither tool attaches to a process, finds a target, reads game files, or wires
the result to scene construction, components, camera, rendering, input, or
normal startup. A separate reviewed native behavior contract is required before
the runtime can use this evidence.

## Native admission

`ReviewedSceneManagerLifecycleContract::load_local` admits only the direct,
regular, non-symlink file `reviewed-scene-manager-lifecycle.json` in a chosen
private directory. It accepts the exact bounded v1 receipt schema: two fixed
successful categorical records and one fixed failure record. It rejects missing
or extra fields, duplicate keys, JSON escapes/non-ASCII strings, trailing
bytes, directories, symlinks, and files larger than 64 KiB.

The native type retains only
`SceneManagerLifecycleReceipt::repeated_success_with_distinct_failure`. It
does not retain a callback ordinal, scene identity, data identity, asset,
address, raw observation, timing, or service. No runtime activation is wired
to this receipt.

## Disconnected activation gate

`ReviewedSceneActivationGate` is a deliberately disconnected, one-shot commit
boundary that can consume the inert receipt when a later behavior-specific
adapter is independently reviewed. It accepts an opaque caller-owned staged
lease and an explicit callback; neither value carries a scene name, object
identity, callback ordinal, camera route, renderer, input route, or gameplay
state. The gate supplies no manager operation itself.

It rejects a missing receipt, missing staged lease, missing callback, failed
callback, repeat commit, or recursive use. It keeps the candidate uncommitted
on failure and retains only its opaque lease after a successful callback. This
is a portable safety transaction, not evidence that any particular retail
scene may be activated. Normal startup does not construct or call this gate.
