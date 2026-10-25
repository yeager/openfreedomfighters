# Picture bounds

Picture bounds are a renderer-neutral geometry service. The caller supplies
validated geometry and explicit dimensions; the service returns bounds without
owning resource loading, scene admission, or ancestor propagation.

Bounds calculation and application to a runtime object are separate operations.
The host supplies resource-query and invalidation interfaces, preserves object
identity, and controls the order in which resulting state is published. Missing
or invalid inputs fail closed according to project safety policy. The API does
not infer dimensions, transforms, or parent updates from unrelated resources.

## Verification

Use independently authored geometry fixtures to cover grouped and overlapping
ranges, empty or invalid input, finite-result checks, query success and failure,
and invalidation ordering. Tests must not contain installation descriptors or
expected values derived from them. A successful bounds calculation alone does
not establish complete scene propagation or rendered output.
