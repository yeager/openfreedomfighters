# Startup scene resource and reader boundary

This document describes project implementation boundaries only. It does not
publish installation-derived resource identities, counts, payload summaries, or
private observations.

## Runtime boundary

Startup resource verification, reader dispatch, component lifecycle, scene
activation, and presentation are distinct operations. Each transition must use
validated project interfaces and retain ownership explicitly. A diagnostic
resource view is not evidence that the normal scene is active or interactive.

The public implementation must not expose installation identifiers or payload
summaries as an inventory. Tests use synthetic, independently authored input
and cover malformed framing, boundary conditions, and failure handling.

## Presentation boundary

A normal menu may be presented only after the project has implemented and tested
its activation lifecycle, reader dispatch, camera and draw interfaces, input
routing, and teardown. A successful resource parse alone does not authorize
input, focus, rendering, or gameplay transitions. Keep diagnostics separate from
normal presentation until those contracts are complete.
