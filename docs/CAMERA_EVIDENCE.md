# Camera and projection boundary

The normal gameplay camera and its submitted view-projection path are not yet connected to the production renderer. This page intentionally omits observations and source-derived projection values.

A future camera implementation must define coordinate conventions, projection, viewport, aspect handling, clipping, and cut/resize invalidation in a reviewed source-free behavior contract. Diagnostic camera fits are not evidence of gameplay camera selection. Synthetic tests should cover finite inputs and boundary conditions.
