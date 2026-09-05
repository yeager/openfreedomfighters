# Camera and projection evidence boundary

This document records camera behavior established through clean-room analysis.
It is a behavior specification, not a transcription of the original
implementation, and it does not establish a complete Original-mode camera.

## Established behavior

The camera model accepts independently configurable field of view, near and far
clip distances, physical viewport dimensions, and an aspect multiplier. Its
perspective path converts field of view from degrees to radians, evaluates the
half angle, and derives the conventional reciprocal half-angle perspective
scale. Viewport height divided by viewport width, combined with the explicit
aspect multiplier, determines the corresponding aspect adjustment.

Projection setup also constructs six normalized four-component frustum planes.
These planes are camera-owned culling state; their existence does not establish
the memory layout of the matrix submitted to a GPU. Screen conversion uses the
stored horizontal and vertical projection scales and viewport dimensions, with
the vertical screen coordinate inverted for top-left-oriented presentation.

An explicit 4:3 compatibility multiplier is part of the observed aspect policy.
It belongs behind a compatibility/profile choice and must not be imposed on
Modern or Modern+ output unconditionally.

## Unresolved conventions

The following remain open evidence requirements:

- final projection-matrix memory layout and multiplication side;
- world/view handedness;
- clip-space depth range and the exact near/far coefficients;
- whether the camera pose is stored or submitted as view or inverse-view;
- active-camera selection and update timing; and
- the relationship between camera frustum state and renderer constant uploads.

Until these are established, the current bounds-normalized diagnostic projection
must not be described as the original camera. A future perspective prototype may
exercise the proven scalar relationships, but it remains diagnostic unless its
view and depth conventions are independently validated.

## Validation gate

An Original-mode implementation requires synthetic arithmetic tests for FOV and
aspect changes, near/far clipping, all six plane orientations, screen-coordinate
conversion, and degenerate viewport rejection. Numeric runtime observations must
then establish matrix layout, handedness, depth mapping, and view convention
before cross-backend image comparisons can be treated as fidelity evidence.
