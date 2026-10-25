# Runtime resource state

The runtime owns typed resource identities and lifecycle state. Resource state changes must remain attached to the owning session so stale handles cannot be reused after a scene is retired.

## Boundary

- Validate resource ownership before applying a mutation.
- Keep source identity, runtime identity, and renderer identity distinct.
- Retire leases and GPU resources through their owning lifecycle.
- Reject incomplete or ambiguous state rather than inventing a default scene.

This document intentionally omits retail-derived flags, identifiers, class values, object layouts, and private analysis. See the runtime interfaces in `include/off/graphics` and the publication policy in [CLEAN_ROOM.md](../CLEAN_ROOM.md).
