# Picture expansion

Picture expansion is a CPU-side geometry operation that converts validated
project picture inputs into an owned draw plan. It does not own scene activation,
resource loading, final material state, projection, GPU submission, or
presentation.

## Portable policy

The caller supplies the transform and rendering state explicitly. Expansion
preserves authored project geometry and ordering; it must not invent missing
runtime values. Project-owned limits bound input and output allocations.
Invalid or non-finite input fails closed according to native safety policy.
These limits describe the portable implementation only.

## Integration and verification

A caller must justify each transform and provide explicit projection and
material inputs. The renderer remains responsible for depth, clipping, sampling,
blend state, and output mapping. Tests use independently authored fixtures for
geometry, ordering, invalid inputs, and bounds. Do not publish installation
geometry, derived equations, resource joins, or expected outputs from private
analysis. A CPU draw plan does not prove visual parity.
