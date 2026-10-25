# Picture fade receiver boundary

The picture-fade receiver is a CPU-side state transition component. It accepts
explicit events and timing inputs and emits effects through a caller-provided
interface. It does not load resources, admit a scene, schedule updates, or submit
graphics work.

The host owns event naming, clock production, owner state, resource lifetime,
and delivery of emitted effects to rendering. Keep these responsibilities
separate so that updating CPU state is not mistaken for a rendered transition.
Unsupported inputs and callback failures follow explicit project safety policy;
the API does not claim to reproduce undocumented engine error handling.

## Integration and verification

Tests use synthetic owners, events, clocks, and effect visitors. They should
cover ordering, repeated events, boundary times, invalid inputs, and callback
failure without relying on installation content. A passing receiver test does
not establish scene activation, resource loading, draw admission, or visual
parity. Keep any private observations and captures outside the repository.
