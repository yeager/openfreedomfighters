# Spatial-resource handling

Spatial resources are parsed into bounded hierarchy, bounds, descriptor, and
reference models. Traversal validates ownership and range relationships before
returning candidate records to higher-level code.

A parsed spatial descriptor is not by itself a world-space transform or proof
that the referenced object reaches the normal renderer. Transform composition
and draw admission require an independently reviewed behavior contract.

This document intentionally omits game-specific layouts, encodings, values,
corpus measurements, and executable-analysis results.
