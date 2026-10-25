# Window-picture transforms

The renderer-neutral picture-transform module accepts explicit hierarchy,
viewport, alignment, and object-state inputs and prepares values for a draw
visitor. It does not infer scene matrices from unrelated serialized links or own
resource loading, scene admission, or presentation policy.

## Contract boundaries

Keep hierarchy production, alignment, cache preparation, and descriptor
expansion as separately testable operations. The host supplies initialized
runtime state and explicit policies for values unavailable to the portable
renderer. Invalid or non-finite inputs fail closed under project safety policy.
A cached result must be invalidated whenever any dependency changes.

The startup renderer may use a logical canvas and output mapping as project
policy. Such choices must be documented as portable presentation decisions,
not as compatibility findings.

## Verification

Use independently authored synthetic hierarchies and geometry to test parent
ordering, invalid graphs, cache invalidation, viewport mapping, and numerical
boundaries. Do not publish executable-derived formulas, private probes, or
installation-backed expected vectors here. CPU transform tests do not prove
scene activation or pixel parity.
