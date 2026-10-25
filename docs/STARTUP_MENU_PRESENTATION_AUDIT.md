# Startup-menu presentation audit

This page records project implementation boundaries, not observations of a
commercial game or its assets.

## Current native surface

A diagnostic startup view may be used to inspect project-owned rendering and
resource-decoding code. It is not the interactive menu, and it must not be used
to imply that the application has completed menu activation or input handling.
Normal startup and diagnostic presentation remain separate paths until a
reviewed, source-free behavior contract supports their integration.

## Integration gates

Before a normal interactive menu is considered complete, project-authored work
must define and test:

1. scene activation and teardown ownership;
2. resource-reader and component-lifecycle boundaries;
3. camera, viewport, and draw-submission interfaces;
4. focus, navigation, and action dispatch; and
5. transition handling that prevents stale input, rendering, or audio work.

Tests should use synthetic data and independently authored fixtures. Diagnostics
must not export installation assets, identifiers, text, screenshots, or private
analysis. A static image or successful renderer submission alone does not prove
an interactive menu or visual parity.
