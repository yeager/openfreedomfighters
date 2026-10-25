# Geometry-resource handling

The geometry reader exposes validated indexed geometry to the portable rendering
pipeline. It checks record bounds, vertex and index references, topology ranges,
and cross-resource material references before GPU preparation.

Diagnostic rendering is not evidence of original placement, camera selection,
material semantics, or complete scene admission. This document intentionally
omits game-specific layouts, values, corpus measurements, and geometry payloads.
