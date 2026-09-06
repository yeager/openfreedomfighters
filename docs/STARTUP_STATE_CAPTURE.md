# Original startup state observation

Static research establishes individual material setters and state requests,
but not their complete execution history before the first startup picture.
Do not substitute renderer reset values or load-time material properties for
observed draw-time state.

## Private Windows observation

Use the legally owned original on a supported Windows setup. Keep executable
coordinates, memory dumps, identities, traces and captured images outside the
public repository. Do not bypass data/ownership verification or change branch
inputs to force a preferred outcome.

At the first actual startup picture descriptor callback, record:

1. The selected renderer/callback family and resource subtype/mode.
2. Both pass-disable masks, their effective features, suppression byte and
   mode selector.
3. Incoming material/secondary/threshold and cached triple before and after
   the material request; classify suppressed, cache-hit, zero-feature or
   active requests.
4. The preceding resource-binding event, if present, and its stage-zero
   texture/color/alpha requests.
5. Ordered writes to that resource from load through drawing, distinguishing
   load-time refresh, alpha update, explicit override and other writers.
   Private stable identities must distinguish shared records from copies.

Correlate these events with the actual picture draw, not merely the first
frame counter or first scene load. Repeat in a fresh process without deliberate
input and record configuration, resolution, executable/data hashes and tool
versions. Compare the two runs before promoting a result to a behavior-only
specification. This establishes only the selected startup configuration, not
all subsequent menu states, animations or resolutions.

## Placement and cache observation

The material-state procedure above alone does not establish picture placement.
For the same exact draw, additionally capture these fields privately:

1. Selected camera/view identity and ordinary/alternate branch; raw near,
   selected far, angle or alternate scalars, virtual aspect, renderer dimension
   query values, ordinary renderer scalar, and selected depth-matrix variant.
2. Pass rectangle, normalized viewport request, computed floating-point
   viewport, converted integer viewport, depth range and render-target size.
3. Produced projection matrix and the actual WORLD/VIEW/PROJECTION matrices
   and viewport active at submission.
4. Submission and aligned-local positions, picture extents, virtual-window
   scale, owner projection scalar, object/hierarchy basis and translation,
   alignment enum and picture/owner half-extents.
5. The inherited Y-basis operand actually consumed during preparation,
   including whether it is finite. Do not relabel it as a renderer query.
6. Group-table attachment presence and cache dirty flag, position, basis and
   translation before and after submission. Distinguish reuse from recomputation.
7. Ordered invalidations and dependency changes since the previous preparation.
   A reused cache needs that earlier preparation event, not only the current
   callback's inputs.
8. Emitted positions/UVs and their indexed-draw association, so every captured
   dependency can be correlated with the selected draw rather than a nearby one.

Keep these captures and original vertex data out of the repository. Repeat and
independently review the resulting behavior-only conclusions. Even complete
placement evidence does not prove clipping, sampling, compositing, full pixel
fidelity, or later-frame behavior.

## Current limits

The current development host is ARM64 Linux; this Windows observation has not
been performed here. The native reimplementation does not need an emulator,
but research evidence from the original remains necessary for this boundary.
Independent CPU components can continue to be implemented from reviewed
behavioral contracts while final startup renderer wiring remains gated.

Reset initializes the material cache to `(0xffffffff, 0, 0)`, but preceding
passes can alter it. Setup clears suppression, while another pass temporarily
sets it and later modifies the cache. Input can change feature masks. Alpha
updates can modify material bit 0x1 and propagate it to resources. None of
these facts alone proves which state reaches the selected first draw.
