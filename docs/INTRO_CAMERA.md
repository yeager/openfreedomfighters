# Intro camera boundary

The normal intro path does not yet establish an authored active camera or connect a verified view transform to accepted scene draws. A preview projection or diagnostic camera is not evidence of the original camera behavior.

Camera admission must be owned by the live scene session, follow the scene lifecycle, and feed the same frame that produces geometry and motion data. Camera changes and scene transitions must invalidate temporal history before any temporal or vendor upscaler is enabled.

This public document omits private executable traces, camera values, flags, layouts, and retail data. See [TRANSFORM_BOUNDARY.md](TRANSFORM_BOUNDARY.md) and [MODERN_GRAPHICS.md](MODERN_GRAPHICS.md).
