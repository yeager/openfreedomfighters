# Private scene-manager lifecycle contract

`scene_manager_lifecycle_trace.py` accepts one private structural observation of
the scene-manager boundary. Its exact schema retains only phase ordering,
observer-local callback ordinal, categorical staging/lifecycle/camera/commit
results, and outcome. It rejects paths, addresses, identities, text, bytes,
timings, assets, and extra fields.

`scene_manager_lifecycle_contract_bundle.py` requires two identical successful
observations and a structurally separate failed observation. It emits only the
fixed inert receipt `off.scene-manager-lifecycle-contract/v1`; the receipt
does not preserve raw observations or any retail material.

Neither tool attaches to a process, finds a target, reads game files, or wires
the result to scene construction, components, camera, rendering, input, or
normal startup. A separate reviewed native behavior contract is required before
the runtime can use this evidence.
